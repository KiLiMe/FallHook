// AI CONTEXT: Builds and resolves INFO:NAM1 subtitle fallback maps for 1.10.163.
// Depends on RuntimeFormResolver, RuntimeLocalizedStringID, response candidates, and TranslationCatalog.
// Runtime scope is Fallout 4 1.10.163 subtitle display paths with TESTopicInfo identity available.
// Version-specific logic: uses 110163 form resolution and TESTopicInfo candidate traversal.
// Source-free policy: matches by INFO form and sID only; live text is diagnostic input, not lookup identity.
#include "PCH.h"

#include "110163/RuntimeDialogueSubtitleTranslations.h"

#include "RuntimeApplySettings.h"
#include "110163/RuntimeDialogueResponseCandidates.h"
#include "110163/RuntimeFormResolver.h"
#include "110163/RuntimeLocalizedStringID.h"

#include "RE/T/TESTopic.h"
#include "RE/T/TESTopicInfo.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace
{
	constexpr std::uint32_t kTraceLimit{ 1024 };

	struct Target
	{
		std::string text;
		RE::BSFixedStringCS fixedText;
	};

	struct InfoTargets
	{
		std::unordered_map<std::uint32_t, Target> bySID;
		Target uniqueTarget;
		bool hasUnique{ false };
		bool ambiguousUnique{ false };
	};

	struct SIDTarget
	{
		Target target;
		bool ambiguous{ false };
	};

	struct Snapshot
	{
		std::unordered_map<std::uint32_t, InfoTargets> byInfoForm;
		std::unordered_map<std::uint32_t, SIDTarget> bySID;
		bool traceEnabled{ false };
	};

	std::mutex g_rebuildLock;
	const TranslationCatalogBuildResult* g_catalog{ nullptr };
	std::atomic<std::shared_ptr<const Snapshot>> g_snapshot;
	std::atomic_uint32_t g_traceLines{ 0 };

	[[nodiscard]] bool hasText(std::string_view text)
	{
		return text.find_first_not_of(" \t\r\n") != std::string_view::npos;
	}

	[[nodiscard]] bool isSubtitleRecord(const TranslationCatalogRecord& record)
	{
		return record.recordSignature == "INFO NAM1" &&
			record.data.translationType == TranslationType::kRuntimeIndex &&
			hasText(record.data.replacerText);
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

	[[nodiscard]] Target makeTarget(std::string text)
	{
		Target target;
		target.text = std::move(text);
		target.fixedText = target.text;
		return target;
	}

	void addUnique(InfoTargets& targets, const Target& target)
	{
		if (!targets.hasUnique)
		{
			targets.uniqueTarget = target;
			targets.hasUnique = true;
			return;
		}
		if (targets.uniqueTarget.text != target.text)
		{
			targets.uniqueTarget = {};
			targets.ambiguousUnique = true;
		}
	}

	void addSID(Snapshot& snapshot, std::uint32_t sid, const Target& target)
	{
		if (sid == 0)
		{
			return;
		}
		auto [it, inserted] = snapshot.bySID.try_emplace(sid, SIDTarget{ target, false });
		if (!inserted && it->second.target.text != target.text)
		{
			it->second.target = {};
			it->second.ambiguous = true;
		}
	}

	[[nodiscard]] const Target* resolveInfoTarget(
		const Snapshot& snapshot,
		const RE::TESTopicInfo* topicInfo,
		const RuntimeDialogueResponseCandidates::TopicInfoCandidates& candidates,
		std::optional<std::uint32_t> sid,
		std::uint32_t& matchedForm,
		std::string_view& mode)
	{
		for (std::size_t i = 0; sid && i < candidates.count; ++i)
		{
			const auto* info = candidates.values[i];
			const auto foundInfo = info ? snapshot.byInfoForm.find(info->formID) : snapshot.byInfoForm.end();
			if (foundInfo == snapshot.byInfoForm.end())
			{
				continue;
			}
			if (const auto found = foundInfo->second.bySID.find(*sid); found != foundInfo->second.bySID.end())
			{
				matchedForm = info->formID;
				mode = "INFO+sID";
				return std::addressof(found->second);
			}
		}

		const auto foundExact = topicInfo ? snapshot.byInfoForm.find(topicInfo->formID) : snapshot.byInfoForm.end();
		if (foundExact != snapshot.byInfoForm.end())
		{
			const auto& targets = foundExact->second;
			if (targets.hasUnique && !targets.ambiguousUnique)
			{
				matchedForm = topicInfo->formID;
				mode = "INFO:unique";
				return std::addressof(targets.uniqueTarget);
			}
		}
		return nullptr;
	}

	void trace(
		const Snapshot& snapshot,
		std::string_view stage,
		const RE::TESTopicInfo* topicInfo,
		std::uint32_t matchedForm,
		std::optional<std::uint32_t> sid,
		std::string_view mode,
		std::size_t liveLen,
		std::size_t textLen)
	{
		if (!snapshot.traceEnabled || g_traceLines.fetch_add(1, std::memory_order_relaxed) >= kTraceLimit)
		{
			return;
		}
		REX::INFO(
			"{} dialogue-subtitle trace stage={} info={:08X} matched={:08X} sid={} mode={} liveLen={} textLen={}",
			Plugin::NAME,
			stage,
			topicInfo ? topicInfo->formID : 0,
			matchedForm,
			sid.value_or(0),
			mode,
			liveLen,
			textLen);
	}
}

namespace RuntimeDialogueSubtitleTranslations
{
	void Rebuild(const TranslationCatalogBuildResult& catalog, bool force)
	{
		std::scoped_lock lock{ g_rebuildLock };
		if (!force && g_catalog == std::addressof(catalog))
		{
			return;
		}

		auto snapshot = std::make_shared<Snapshot>();
		snapshot->traceEnabled = RuntimeApplySettings::Load().TraceEnabled();
		snapshot->byInfoForm.reserve(catalog.records.size() / 4);

		for (const auto& record : catalog.records)
		{
			if (!isSubtitleRecord(record))
			{
				continue;
			}
			const auto formID = runtimeFormID(record);
			const auto target = makeTarget(record.data.replacerText);
			if (!formID)
			{
				if (record.data.stringID)
				{
					addSID(*snapshot, *record.data.stringID, target);
				}
				continue;
			}
			auto& targets = snapshot->byInfoForm[*formID];
			addUnique(targets, target);
			if (record.data.stringID)
			{
				targets.bySID.insert_or_assign(*record.data.stringID, target);
				addSID(*snapshot, *record.data.stringID, target);
			}
		}

		g_catalog = std::addressof(catalog);
		g_traceLines.store(0, std::memory_order_relaxed);
		g_snapshot.store(std::static_pointer_cast<const Snapshot>(snapshot), std::memory_order_release);
		if (snapshot->traceEnabled)
		{
			REX::INFO("{} dialogue-subtitle map built: infos={} sids={}.",
				Plugin::NAME, snapshot->byInfoForm.size(), snapshot->bySID.size());
		}
	}

	LookupResult Resolve(RE::TESTopicInfo* topicInfo, std::string_view liveText)
	{
		const auto snapshot = g_snapshot.load(std::memory_order_acquire);
		if (!snapshot || !topicInfo)
		{
			return {};
		}

		auto* topic = topicInfo->parentTopic;
		const auto candidates = RuntimeDialogueResponseCandidates::Make(topic, topicInfo);
		const auto sid = RuntimeLocalizedStringID::Parse(liveText);
		std::uint32_t matchedForm = 0;
		std::string_view mode;

		const auto* target = resolveInfoTarget(*snapshot, topicInfo, candidates, sid, matchedForm, mode);
		if (!target && sid)
		{
			const auto found = snapshot->bySID.find(*sid);
			if (found != snapshot->bySID.end() && !found->second.ambiguous)
			{
				target = std::addressof(found->second.target);
				mode = "sID:unique";
			}
		}

		if (!target || target->text.empty())
		{
			trace(*snapshot, "miss", topicInfo, 0, sid, "none", liveText.size(), 0);
			return {};
		}

		trace(*snapshot, "hit", topicInfo, matchedForm, sid, mode, liveText.size(), target->text.size());
		return { target->fixedText, mode, matchedForm, true };
	}
}
