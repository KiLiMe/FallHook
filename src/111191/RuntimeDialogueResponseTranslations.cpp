// AI CONTEXT: Builds and resolves INFO:NAM1 maps during DialogueResponse construction.
// Depends on RuntimeFormResolver, RuntimeLocalizedStringID, response candidates, and CommonLibF4 dialogue types.
// Runtime scope is Fallout 4 1.11.191 response identity resolution before subtitle-context capture.
// Version-specific logic: uses 111191 resolver and dialogue layouts; no hook addresses live here.
// Source-free policy: resolves by INFO form, response sID, ordinal, and response ID/index candidates; never by Source text.
#include "PCH.h"

#include "111191/RuntimeDialogueResponseTranslations.h"

#include "RuntimeApplySettings.h"
#include "RuntimeActivityWatch.h"
#include "111191/RuntimeFormResolver.h"
#include "111191/RuntimeDialogueResponseCandidates.h"
#include "111191/RuntimeDialogueResponseMap.h"
#include "111191/RuntimeLocalizedStringID.h"

#include "RE/B/BSFixedString.h"
#include "RE/T/TESResponse.h"
#include "RE/T/TESTopicInfo.h"

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
	constexpr std::uint32_t kTraceLimit{ 192 };
	namespace ResponseMap = RuntimeDialogueResponseMap;

	struct ResponseResolution
	{
		std::uint64_t generation{ 0 };
		std::string text;
		RE::BSFixedStringCS fixedText;
		std::uint32_t ordinal{ 0xFFFFFFFFu };
		std::uint32_t responseID{ 0 };
		bool translated{ false };
	};

	struct ResponseCacheKey
	{
		const RE::TESTopic* topic{ nullptr };
		const RE::TESTopicInfo* topicInfo{ nullptr };
		const RE::TESResponse* response{ nullptr };

		[[nodiscard]] bool operator==(const ResponseCacheKey&) const = default;
	};

	struct ResponseCacheKeyHash
	{
		[[nodiscard]] std::size_t operator()(const ResponseCacheKey& key) const noexcept
		{
			const auto topicValue = reinterpret_cast<std::uintptr_t>(key.topic);
			const auto infoValue = reinterpret_cast<std::uintptr_t>(key.topicInfo);
			const auto responseValue = reinterpret_cast<std::uintptr_t>(key.response);
			const auto mixedInfo = infoValue + 0x9E3779B97F4A7C15ull + (responseValue << 6) + (responseValue >> 2);
			return std::hash<std::uintptr_t>{}(responseValue ^ mixedInfo ^ (topicValue << 1));
		}
	};

	std::mutex g_rebuildLock;
	std::shared_mutex g_responseCacheLock;
	const TranslationCatalogBuildResult* g_catalog{ nullptr };
	std::atomic<std::shared_ptr<const ResponseMap::Snapshot>> g_snapshot;
	std::unordered_map<ResponseCacheKey, std::shared_ptr<ResponseResolution>, ResponseCacheKeyHash> g_responseCache;
	std::atomic_uint32_t g_traceLines{ 0 };
	std::atomic_uint64_t g_generation{ 0 };

	[[nodiscard]] bool isResponseRecord(const TranslationCatalogRecord& record) noexcept
	{
		return record.recordSignature == "INFO NAM1" && record.data.translationType == TranslationType::kRuntimeIndex &&
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

	void traceApply(const ResponseMap::Snapshot& snapshot,
		std::string_view stage, RE::TESTopicInfo* info, std::uint32_t ordinal, std::uint32_t id, std::size_t textLen)
	{
		if (!snapshot.traceEnabled || g_traceLines.fetch_add(1, std::memory_order_relaxed) >= kTraceLimit)
		{
			return;
		}
		REX::INFO("{} dialogue-response trace stage={} info={:08X} ordinal={} responseID={} textLen={}",
			Plugin::NAME, stage, info ? info->formID : 0, ordinal, id, textLen);
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

	[[nodiscard]] std::shared_ptr<ResponseResolution> cacheResponse(const ResponseCacheKey& key, const std::shared_ptr<ResponseResolution>& resolution)
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
		const ResponseMap::Snapshot& snapshot, RE::TESTopic* topic, RE::TESTopicInfo* topicInfo, RE::TESResponse* response)
	{
		auto resolution = std::make_shared<ResponseResolution>();
		resolution->generation = snapshot.generation;

		const auto candidates = RuntimeActivityWatch::RunWork("DialogueResponse candidates", [&]() { return RuntimeDialogueResponseCandidates::Make(topic, topicInfo); });
		const auto owner = RuntimeActivityWatch::RunWork("DialogueResponse owner scan", [&]() {
			return RuntimeDialogueResponseCandidates::FindOwner(candidates, response);
		});
		const auto sid = RuntimeActivityWatch::RunWork("DialogueResponse SID read", [&]() { return RuntimeLocalizedStringID::Read(response->responseText); });

		if (owner)
		{
			resolution->ordinal = owner->ordinal;
		}
		const auto* target = owner && sid ? RuntimeActivityWatch::RunWork("DialogueResponse owner SID lookup",
												 [&]() { return ResponseMap::LookupInfoSID(snapshot, owner->info, *sid); }) : nullptr;
		if (!target && topicInfo)
		{
			target = sid ? RuntimeActivityWatch::RunWork("DialogueResponse topic SID lookup",
							   [&]() { return ResponseMap::LookupInfoSID(snapshot, topicInfo, *sid); }) :
						   RuntimeActivityWatch::RunWork("DialogueResponse topic default lookup",
							   [&]() { return ResponseMap::LookupInfoDefault(snapshot, topicInfo); });
		}
		if (!target && sid)
		{
			target = RuntimeActivityWatch::RunWork("DialogueResponse SID lookup", [&]() { return ResponseMap::LookupBySID(snapshot, candidates, *sid); });
		}
		if (!target && topic && sid)
		{
			target = RuntimeActivityWatch::RunWork("DialogueResponse topic form SID lookup", [&]() { return ResponseMap::LookupFormSID(snapshot, topic->formID, *sid); });
		}
		if (!target)
		{
			target = RuntimeActivityWatch::RunWork("DialogueResponse unique SID lookup", [&]() -> const ResponseMap::Target* {
				const auto found = sid ? snapshot.byUniqueSID.find(*sid) : snapshot.byUniqueSID.end();
				return found != snapshot.byUniqueSID.end() && !found->second.text.empty() ? std::addressof(found->second) : nullptr;
			});
		}
		resolution->responseID = RuntimeActivityWatch::RunWork("DialogueResponse responseID read", [&]() { return responseID(response); });
		if (!target && resolution->responseID > 0)
		{
			target = owner ? RuntimeActivityWatch::RunWork("DialogueResponse owner responseID lookup",
								 [&]() { return ResponseMap::LookupInfoResponseID(snapshot, owner->info, resolution->responseID); }) : nullptr;
			if (!target && topicInfo)
			{
				target = RuntimeActivityWatch::RunWork("DialogueResponse exact info responseID lookup",
					[&]() { return ResponseMap::LookupInfoResponseID(snapshot, topicInfo, resolution->responseID); });
			}
			if (!target)
			{
				target = RuntimeActivityWatch::RunWork("DialogueResponse candidate responseID lookup",
					[&]() { return ResponseMap::LookupByResponseID(snapshot, candidates, resolution->responseID); });
			}
		}
		if (!target && resolution->responseID > 0)
		{
			std::size_t indexCount = 0;
			const auto indexes = ResponseMap::IndexCandidates(
				owner ? owner->ordinal : 0xFFFFFFFFu,
				resolution->responseID,
				indexCount);
			target = RuntimeActivityWatch::RunWork("DialogueResponse candidate index lookup",
				[&]() { return ResponseMap::LookupByIndex(snapshot, candidates, indexes, indexCount); });
		}
		if (!target)
		{
			if (owner)
			{
				target = RuntimeActivityWatch::RunWork("DialogueResponse owner index lookup",
					[&]() { return ResponseMap::LookupInfoIndex(snapshot, owner->info, owner->ordinal); });
			}
			if (!target && owner)
			{
				target = RuntimeActivityWatch::RunWork("DialogueResponse owner default lookup", [&]() { return ResponseMap::LookupInfoDefault(snapshot, owner->info); });
			}
			if (!target && topicInfo)
			{
				target = RuntimeActivityWatch::RunWork("DialogueResponse topic default lookup",
					[&]() { return ResponseMap::LookupInfoDefault(snapshot, topicInfo); });
			}
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
		auto snapshot = std::make_shared<ResponseMap::Snapshot>();
		snapshot->traceEnabled = RuntimeApplySettings::Load().TraceEnabled();
		snapshot->generation = g_generation.fetch_add(1, std::memory_order_acq_rel) + 1;
		snapshot->byInfoForm.reserve(catalog.records.size());
		std::size_t exactResponseIDs = 0;
		for (const auto& record : catalog.records)
		{
			if (!isResponseRecord(record))
			{
				continue;
			}
			const auto formID = runtimeFormID(record);
			const auto target = ResponseMap::MakeTarget(record.data.replacerText);
			if (!formID)
			{
				if (record.data.stringID)
				{
					ResponseMap::AddUniqueSID(*snapshot, *record.data.stringID, target);
				}
				continue;
			}
			auto& targets = snapshot->byInfoForm[*formID];
			if (record.data.stringID)
			{
				ResponseMap::AddUniqueSID(*snapshot, *record.data.stringID, target);
				targets.bySID.insert_or_assign(*record.data.stringID, target);
			}
			if (record.data.index)
			{
				targets.byIndex.insert_or_assign(*record.data.index, target);
			}
			else
			{
				ResponseMap::AddDefaultTarget(targets, target);
			}
			if (record.data.responseID)
			{
				ResponseMap::AddResponseIDTarget(targets, *record.data.responseID, target);
				++exactResponseIDs;
			}
		}
		g_catalog = std::addressof(catalog);
		{
			std::unique_lock cacheLock{ g_responseCacheLock };
			g_responseCache.clear();
		}
		g_traceLines.store(0, std::memory_order_relaxed);
		g_snapshot.store(std::static_pointer_cast<const ResponseMap::Snapshot>(snapshot), std::memory_order_release);
		if (snapshot->traceEnabled)
		{
			REX::INFO("{} dialogue-response map built: infoForms={} uniqueSIDs={} exactResponseIDs={}.",
				Plugin::NAME, snapshot->byInfoForm.size(), snapshot->byUniqueSID.size(), exactResponseIDs);
		}
	}

	LookupResult ResolveResponse(RE::TESTopic* topic, RE::TESTopicInfo* topicInfo, RE::TESResponse* response)
	{
		if (!topicInfo || !response)
		{
			return {};
		}
		const auto snapshot = g_snapshot.load(std::memory_order_acquire);
		if (!snapshot)
		{
			return {};
		}
		const ResponseCacheKey key{ topic, topicInfo, response };
		auto resolution = RuntimeActivityWatch::RunWork(
			"DialogueResponse cache lookup",
			[&]() { return findCachedResponse(key, snapshot->generation); });
		if (!resolution)
		{
			resolution = RuntimeActivityWatch::RunWork("DialogueResponse resolve response", [&]() { return resolveResponse(*snapshot, topic, topicInfo, response); });
			resolution = cacheResponse(key, resolution);
		}

		if (!resolution || !resolution->translated || resolution->text.empty())
		{
			return {};
		}

		traceApply(*snapshot, "capture-resolve", topicInfo, resolution->ordinal, resolution->responseID, resolution->text.size());
		return {
			resolution->fixedText,
			resolution->ordinal,
			resolution->responseID,
			true
		};
	}
}


} // namespace Runtime111191
