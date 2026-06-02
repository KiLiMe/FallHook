// AI CONTEXT: Builds and applies INFO:NAM1 maps for DialogueResponse constructor-time mutation.
// Depends on RuntimeFormResolver, RuntimeLocalizedStringID, RuntimeTextStringAssign, and CommonLibF4 dialogue types.
// Runtime scope is Fallout 4 1.11.191 response construction before UI/subtitle consumption.
// Version-specific logic: uses 111191 resolver/string assignment modules; no hook addresses live here.
// Source-free policy: resolves by INFO form, response sID, ordinal, and response ID candidates; never by Source text.
#include "PCH.h"

#include "111191/RuntimeDialogueResponseTranslations.h"

#include "RuntimeApplySettings.h"
#include "RuntimeActivityWatch.h"
#include "111191/RuntimeFormResolver.h"
#include "111191/RuntimeLocalizedStringID.h"
#include "111191/RuntimeTextStringAssign.h"

#include "RE/B/BSFixedString.h"
#include "RE/D/DialogueResponse.h"
#include "RE/T/TESResponse.h"
#include "RE/T/TESTopicInfo.h"

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

namespace Runtime111191
{
namespace
{
	constexpr std::uint32_t kMaxCandidateDepth{ 32 };
	constexpr std::uint32_t kMaxResponses{ 128 };
	constexpr std::uint32_t kTraceLimit{ 192 };

	struct ResponseTargets
	{
		struct Target
		{
			std::string text;
			RE::BSFixedStringCS fixedText;
		};

		std::unordered_map<std::uint32_t, Target> bySID;
		std::unordered_map<std::uint32_t, Target> byIndex;
	};

	struct ResponseSnapshot
	{
		std::unordered_map<std::uint32_t, ResponseTargets> byInfoForm;
		std::uint64_t generation{ 0 };
		bool traceEnabled{ false };
	};

	struct ResponseResolution
	{
		std::uint64_t generation{ 0 };
		std::string text;
		RE::BSFixedStringCS fixedText;
		std::atomic_bool responseAssigned{ false };
		std::uint32_t ordinal{ 0xFFFFFFFFu };
		std::uint32_t responseID{ 0 };
		bool translated{ false };
	};

	struct ResponseCacheKey
	{
		const RE::TESTopicInfo* topicInfo{ nullptr };
		const RE::TESResponse* response{ nullptr };

		[[nodiscard]] bool operator==(const ResponseCacheKey&) const = default;
	};

	struct ResponseCacheKeyHash
	{
		[[nodiscard]] std::size_t operator()(const ResponseCacheKey& key) const noexcept
		{
			const auto infoValue = reinterpret_cast<std::uintptr_t>(key.topicInfo);
			const auto responseValue = reinterpret_cast<std::uintptr_t>(key.response);
			return std::hash<std::uintptr_t>{}(
				responseValue ^ (infoValue + 0x9E3779B97F4A7C15ull + (responseValue << 6) + (responseValue >> 2)));
		}
	};

	struct TopicInfoCandidates
	{
		std::array<RE::TESTopicInfo*, kMaxCandidateDepth> values{};
		std::size_t count{ 0 };
	};

	std::mutex g_rebuildLock;
	std::shared_mutex g_responseCacheLock;
	const TranslationCatalogBuildResult* g_catalog{ nullptr };
	std::atomic<std::shared_ptr<const ResponseSnapshot>> g_snapshot;
	std::unordered_map<ResponseCacheKey, std::shared_ptr<ResponseResolution>, ResponseCacheKeyHash> g_responseCache;
	std::atomic_uint32_t g_traceLines{ 0 };
	std::atomic_uint64_t g_generation{ 0 };

	[[nodiscard]] bool isResponseRecord(const TranslationCatalogRecord& record) noexcept
	{
		return record.recordSignature == "INFO NAM1" &&
			record.data.translationType == TranslationType::kRuntimeIndex &&
			!record.data.replacerText.empty();
	}

	[[nodiscard]] std::optional<std::uint32_t> runtimeFormID(const TranslationCatalogRecord& record)
	{
		if (record.data.formID)
		{
			return RuntimeFormResolver::ResolveRawFormID(*record.data.formID, record.pluginName);
		}
		if (auto* form = RuntimeFormResolver::ResolveEditorForm(record.data))
		{
			return form->formID;
		}
		return std::nullopt;
	}

	[[nodiscard]] std::uint32_t responseID(const RE::TESResponse* response) noexcept
	{
		constexpr std::size_t kResponseIDOffset{ 0x12 };
		if (!response)
		{
			return 0;
		}
		static_assert(sizeof(RE::TESResponse) == 0x18);
		return *(reinterpret_cast<const std::uint8_t*>(response) + kResponseIDOffset);
	}

	[[nodiscard]] ResponseTargets::Target makeTarget(std::string text)
	{
		ResponseTargets::Target target;
		target.text = std::move(text);
		target.fixedText = target.text;
		return target;
	}

	void pushCandidate(TopicInfoCandidates& candidates, RE::TESTopicInfo* info)
	{
		if (!info)
		{
			return;
		}
		for (std::size_t i = 0; i < candidates.count; ++i)
		{
			if (candidates.values[i] == info)
			{
				return;
			}
		}
		if (candidates.count < candidates.values.size())
		{
			candidates.values[candidates.count++] = info;
		}
	}

	void appendCandidateChain(TopicInfoCandidates& candidates, RE::TESTopicInfo* info)
	{
		for (auto* current = info; current && candidates.count < candidates.values.size(); current = current->dataInfo)
		{
			pushCandidate(candidates, current);
		}
	}

	[[nodiscard]] TopicInfoCandidates topicInfoCandidates(RE::TESTopicInfo* topicInfo)
	{
		TopicInfoCandidates candidates;
		auto* responseInfo = topicInfo;
		for (std::uint32_t depth = 0; responseInfo && responseInfo->dataInfo && depth < kMaxCandidateDepth; ++depth)
		{
			responseInfo = responseInfo->dataInfo;
		}
		pushCandidate(candidates, responseInfo);
		appendCandidateChain(candidates, topicInfo);
		appendCandidateChain(candidates, responseInfo);
		return candidates;
	}

	void addIndexCandidate(std::array<std::uint32_t, 5>& indexes, std::size_t& count, std::uint32_t value)
	{
		if (std::ranges::find(indexes.begin(), indexes.begin() + count, value) == indexes.begin() + count)
		{
			indexes[count++] = value;
		}
	}

	[[nodiscard]] std::array<std::uint32_t, 5> indexCandidates(std::uint32_t ordinal, std::uint32_t id, std::size_t& count)
	{
		std::array<std::uint32_t, 5> indexes{};
		addIndexCandidate(indexes, count, ordinal);
		if (id > 0)
		{
			addIndexCandidate(indexes, count, id - 1);
			addIndexCandidate(indexes, count, id);
		}
		if (id > 1)
		{
			addIndexCandidate(indexes, count, id - 2);
		}
		return indexes;
	}

	[[nodiscard]] std::optional<std::uint32_t> findOrdinal(const TopicInfoCandidates& candidates, const RE::TESResponse* response)
	{
		if (!response)
		{
			return std::nullopt;
		}
		for (std::size_t i = 0; i < candidates.count; ++i)
		{
			const auto* candidate = candidates.values[i];
			std::uint32_t ordinal = 0;
			for (auto* current = candidate ? candidate->responses.head : nullptr;
				 current && ordinal < kMaxResponses;
				 current = current->pNext, ++ordinal)
			{
				if (current == response)
				{
					return ordinal;
				}
			}
		}
		return std::nullopt;
	}

	[[nodiscard]] const ResponseTargets::Target* lookupBySID(
		const ResponseSnapshot& snapshot,
		const TopicInfoCandidates& candidates,
		std::uint32_t sid)
	{
		for (std::size_t i = 0; i < candidates.count; ++i)
		{
			const auto* candidate = candidates.values[i];
			if (!candidate)
			{
				continue;
			}
			const auto foundTargets = snapshot.byInfoForm.find(candidate->formID);
			if (foundTargets == snapshot.byInfoForm.end())
			{
				continue;
			}
			if (const auto found = foundTargets->second.bySID.find(sid); found != foundTargets->second.bySID.end())
			{
				return std::addressof(found->second);
			}
		}
		return nullptr;
	}

	[[nodiscard]] const ResponseTargets::Target* lookupByIndex(
		const ResponseSnapshot& snapshot,
		const TopicInfoCandidates& candidates,
		const std::array<std::uint32_t, 5>& indexes,
		std::size_t indexCount)
	{
		for (std::size_t candidateIndex = 0; candidateIndex < candidates.count; ++candidateIndex)
		{
			const auto* candidate = candidates.values[candidateIndex];
			if (!candidate)
			{
				continue;
			}
			const auto foundTargets = snapshot.byInfoForm.find(candidate->formID);
			if (foundTargets == snapshot.byInfoForm.end())
			{
				continue;
			}
			const auto& targets = foundTargets->second;
			for (std::size_t i = 0; i < indexCount; ++i)
			{
				if (const auto found = targets.byIndex.find(indexes[i]); found != targets.byIndex.end())
				{
					return std::addressof(found->second);
				}
			}
		}
		return nullptr;
	}

	void traceApply(
		const ResponseSnapshot& snapshot,
		std::string_view stage,
		RE::TESTopicInfo* info,
		std::uint32_t ordinal,
		std::uint32_t id,
		std::size_t textLen)
	{
		if (!snapshot.traceEnabled || g_traceLines.fetch_add(1, std::memory_order_relaxed) >= kTraceLimit)
		{
			return;
		}
		REX::INFO(
			"{} dialogue-response trace stage={} info={:08X} ordinal={} responseID={} textLen={}",
			Plugin::NAME,
			stage,
			info ? info->formID : 0,
			ordinal,
			id,
			textLen);
	}

	[[nodiscard]] std::shared_ptr<ResponseResolution> findCachedResponse(const ResponseCacheKey& key, std::uint64_t generation)
	{
		std::shared_lock lock{ g_responseCacheLock };
		const auto found = g_responseCache.find(key);
		if (found == g_responseCache.end() || !found->second || found->second->generation != generation)
		{
			return {};
		}
		return found->second;
	}

	[[nodiscard]] std::shared_ptr<ResponseResolution> cacheResponse(
		const ResponseCacheKey& key,
		const std::shared_ptr<ResponseResolution>& resolution)
	{
		std::unique_lock lock{ g_responseCacheLock };
		const auto found = g_responseCache.find(key);
		if (found != g_responseCache.end() && found->second && found->second->generation == resolution->generation)
		{
			return found->second;
		}
		g_responseCache.insert_or_assign(key, resolution);
		return resolution;
	}

	[[nodiscard]] std::shared_ptr<ResponseResolution> resolveResponse(
		const ResponseSnapshot& snapshot,
		RE::TESTopicInfo* topicInfo,
		RE::TESResponse* response)
	{
		auto resolution = std::make_shared<ResponseResolution>();
		resolution->generation = snapshot.generation;

		const auto candidates = RuntimeActivityWatch::RunWork(
			"DialogueResponse candidates",
			[&]() { return topicInfoCandidates(topicInfo); });
		const auto sid = RuntimeActivityWatch::RunWork(
			"DialogueResponse SID read",
			[&]() { return RuntimeLocalizedStringID::Read(response->responseText); });

		const auto* target = sid ? RuntimeActivityWatch::RunWork(
									  "DialogueResponse SID lookup",
									  [&]() { return lookupBySID(snapshot, candidates, *sid); }) :
								  nullptr;
		if (!target)
		{
			resolution->responseID = RuntimeActivityWatch::RunWork(
				"DialogueResponse responseID read",
				[&]() { return responseID(response); });
			const auto ordinal = RuntimeActivityWatch::RunWork(
				"DialogueResponse ordinal scan",
				[&]() { return findOrdinal(candidates, response); });
			std::size_t indexCount = 0;
			std::array<std::uint32_t, 5> indexes{};
			if (ordinal)
			{
				resolution->ordinal = *ordinal;
				indexes = indexCandidates(*ordinal, resolution->responseID, indexCount);
			}
			else if (resolution->responseID > 0)
			{
				addIndexCandidate(indexes, indexCount, resolution->responseID - 1);
				addIndexCandidate(indexes, indexCount, resolution->responseID);
				if (resolution->responseID > 1)
				{
					addIndexCandidate(indexes, indexCount, resolution->responseID - 2);
				}
			}
			target = RuntimeActivityWatch::RunWork(
				"DialogueResponse index lookup",
				[&]() { return lookupByIndex(snapshot, candidates, indexes, indexCount); });
		}
		else if (snapshot.traceEnabled)
		{
			resolution->responseID = responseID(response);
		}

		if (!target || target->text.empty())
		{
			return resolution;
		}

		resolution->translated = true;
		resolution->text = target->text;
		resolution->fixedText = target->fixedText;
		return resolution;
	}
}

namespace RuntimeDialogueResponseTranslations
{
	void Rebuild(const TranslationCatalogBuildResult& catalog, bool force)
	{
		std::scoped_lock lock{ g_rebuildLock };
		if (!force && g_catalog == std::addressof(catalog))
		{
			return;
		}
		auto snapshot = std::make_shared<ResponseSnapshot>();
		snapshot->traceEnabled = RuntimeApplySettings::Load().TraceEnabled();
		snapshot->generation = g_generation.fetch_add(1, std::memory_order_acq_rel) + 1;
		snapshot->byInfoForm.reserve(catalog.records.size());
		for (const auto& record : catalog.records)
		{
			if (!isResponseRecord(record))
			{
				continue;
			}
			const auto formID = runtimeFormID(record);
			if (!formID)
			{
				continue;
			}
			auto& targets = snapshot->byInfoForm[*formID];
			if (record.data.stringID)
			{
				targets.bySID.insert_or_assign(*record.data.stringID, makeTarget(record.data.replacerText));
			}
			if (record.data.index)
			{
				targets.byIndex.insert_or_assign(*record.data.index, makeTarget(record.data.replacerText));
			}
		}
		g_catalog = std::addressof(catalog);
		{
			std::unique_lock cacheLock{ g_responseCacheLock };
			g_responseCache.clear();
		}
		g_traceLines.store(0, std::memory_order_relaxed);
		g_snapshot.store(std::static_pointer_cast<const ResponseSnapshot>(snapshot), std::memory_order_release);
		if (snapshot->traceEnabled)
		{
			REX::INFO("{} dialogue-response map built: infoForms={}.", Plugin::NAME, snapshot->byInfoForm.size());
		}
	}

	bool ApplyConstructedResponse(
		RE::DialogueResponse* constructed,
		RE::TESTopicInfo* topicInfo,
		RE::TESResponse* response)
	{
		if (!constructed || !topicInfo || !response)
		{
			return false;
		}
		const auto snapshot = g_snapshot.load(std::memory_order_acquire);
		if (!snapshot)
		{
			return false;
		}
		const ResponseCacheKey key{ topicInfo, response };
		auto resolution = RuntimeActivityWatch::RunWork(
			"DialogueResponse cache lookup",
			[&]() { return findCachedResponse(key, snapshot->generation); });
		if (!resolution)
		{
			resolution = RuntimeActivityWatch::RunWork(
				"DialogueResponse resolve response",
				[&]() { return resolveResponse(*snapshot, topicInfo, response); });
			resolution = cacheResponse(key, resolution);
		}

		if (!resolution || !resolution->translated || resolution->text.empty())
		{
			return false;
		}

		bool expected = false;
		if (resolution->responseAssigned.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
		{
			RuntimeActivityWatch::RunWork(
				"DialogueResponse assign responseText",
				[&]() { RuntimeTextStringAssign::AssignPlainFixedLocalized(response->responseText, resolution->fixedText); });
		}
		RuntimeActivityWatch::RunWork(
			"DialogueResponse assign constructed text",
			[&]() { constructed->text = resolution->fixedText; });
		traceApply(*snapshot, "ctor-replace", topicInfo, resolution->ordinal, resolution->responseID, resolution->text.size());
		return true;
	}
}

} // namespace Runtime111191
