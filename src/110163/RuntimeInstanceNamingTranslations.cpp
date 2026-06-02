// AI CONTEXT: Builds and applies source-free INNR:WNAM destination rules directly to live naming data.
// Depends on RuntimeFormResolver, RuntimeTextStringAssign, and TranslationCatalog.
// Runtime scope is Fallout 4 1.10.163 BGSInstanceNamingRules rule conditions.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: applies by resolved INNR form ID plus sID/condition identity; never reads Source or generated text.
#include "PCH.h"

#include "InstanceNamingRuleIdentity.h"
#include "PluginEdidIndex.h"
#include "RuntimeApplySettings.h"
#include "110163/RuntimeFormResolver.h"
#include "110163/RuntimeInstanceNamingTranslations.h"
#include "110163/RuntimeLocalizedStringID.h"
#include "110163/RuntimeTextStringAssign.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace
{
	constexpr std::uint32_t kTraceLimit{ 128 };

	struct RuleTranslation
	{
		std::uint32_t ruleSet{ 0 };
		std::uint32_t ruleOffset{ 0 };
		std::uint32_t conditionHash{ 0 };
		std::uint32_t sid{ 0 };
		std::string text;
		bool usesConditionKey{ false };
	};

	struct ApplyTotals
	{
		std::size_t appliedRules{ 0 };
		std::size_t unchangedRules{ 0 };
		std::size_t skippedRuntimeForm{ 0 };
		std::size_t skippedSlotOutOfRange{ 0 };
		std::size_t sidRemappedRules{ 0 };
		std::size_t skippedStringIDMismatch{ 0 };
	};

	struct RuleSlot
	{
		RE::BGSInstanceNamingRules::RuleData* rule{ nullptr };
		std::uint32_t ruleSet{ 0 };
		std::uint32_t ruleOffset{ 0 };
	};

	using RuleConditionIndex = std::array<std::unordered_map<std::uint32_t, RuleSlot>, 10>;

	std::mutex g_lock;
	std::unordered_map<std::uint32_t, std::vector<RuleTranslation>> g_rulesByForm;
	const TranslationCatalogBuildResult* g_rebuiltCatalog{ nullptr };
	RuntimeInstanceNamingTranslations::RebuildStats g_lastRebuildStats;
	std::atomic_bool g_ready{ false };
	std::atomic_bool g_traceEnabled{ false };
	std::atomic_uint32_t g_traceLines{ 0 };

	[[nodiscard]] bool isInstanceNamingRecord(const TranslationCatalogRecord& record)
	{
		return record.data.translationType == TranslationType::kInstanceNamePart &&
			record.recordSignature == "INNR WNAM";
	}

	[[nodiscard]] RE::TESForm* resolveForm(const TranslationCatalogRecord& record)
	{
		if (record.data.formID)
		{
			return RuntimeFormResolver::ResolveRawForm(*record.data.formID, record.pluginName);
		}
		return RuntimeFormResolver::ResolveEditorForm(record.data);
	}

	[[nodiscard]] RE::BGSInstanceNamingRules::RuleData* ruleAtSlot(
		RE::BGSInstanceNamingRules* rules,
		std::uint32_t ruleSet,
		std::uint32_t ruleOffset)
	{
		if (!rules || ruleSet >= std::size(rules->ruleSets))
		{
			return nullptr;
		}

		auto& set = rules->ruleSets[ruleSet];
		const auto count = static_cast<std::uint32_t>(set.size());
		if (ruleOffset >= count)
		{
			return nullptr;
		}
		return std::addressof(set[ruleOffset]);
	}

	[[nodiscard]] std::optional<std::uint32_t> readRuleStringID(const RE::BGSInstanceNamingRules::RuleData& rule)
	{
		return RuntimeLocalizedStringID::Read(rule.text);
	}

	[[nodiscard]] std::optional<RuleSlot> findRuleByStringID(RE::BGSInstanceNamingRules* rules, std::uint32_t sid)
	{
		if (!rules || sid == 0)
		{
			return std::nullopt;
		}

		for (std::uint32_t ruleSet = 0; ruleSet < std::size(rules->ruleSets); ++ruleSet)
		{
			auto& set = rules->ruleSets[ruleSet];
			const auto count = static_cast<std::uint32_t>(set.size());
			for (std::uint32_t ruleOffset = 0; ruleOffset < count; ++ruleOffset)
			{
				auto& rule = set[ruleOffset];
				const auto decodedSid = readRuleStringID(rule);
				if (decodedSid && *decodedSid == sid)
				{
					return RuleSlot{ std::addressof(rule), ruleSet, ruleOffset };
				}
			}
		}
		return std::nullopt;
	}

	[[nodiscard]] std::uint32_t ruleConditionHash(const RE::BGSInstanceNamingRules::RuleData& rule)
	{
		std::vector<std::uint32_t> keywordLocalIDs;
		keywordLocalIDs.reserve(rule.keywords.GetNumKeywords());
		rule.keywords.ForEachKeyword([&](RE::BGSKeyword* keyword) {
			if (keyword)
			{
				keywordLocalIDs.push_back(keyword->formID & 0x00FFFFFFu);
			}
			return RE::BSContainer::ForEachResult::kContinue;
		});
		std::ranges::sort(keywordLocalIDs);
		return InstanceNamingRuleIdentity::HashSortedKeywords(keywordLocalIDs, rule.index);
	}

	[[nodiscard]] RuleConditionIndex buildRuleConditionIndex(RE::BGSInstanceNamingRules* rules)
	{
		RuleConditionIndex index;
		if (!rules)
		{
			return index;
		}
		for (std::uint32_t ruleSet = 0; ruleSet < std::size(rules->ruleSets); ++ruleSet)
		{
			auto& set = rules->ruleSets[ruleSet];
			const auto count = static_cast<std::uint32_t>(set.size());
			for (std::uint32_t ruleOffset = 0; ruleOffset < count; ++ruleOffset)
			{
				auto& rule = set[ruleOffset];
				index[ruleSet].try_emplace(ruleConditionHash(rule), RuleSlot{ std::addressof(rule), ruleSet, ruleOffset });
			}
		}
		return index;
	}

	[[nodiscard]] std::optional<RuleSlot> findRuleByConditionHash(
		const RuleConditionIndex& index,
		std::uint32_t ruleSet,
		std::uint32_t conditionHash)
	{
		if (ruleSet >= index.size())
		{
			return std::nullopt;
		}
		const auto found = index[ruleSet].find(conditionHash);
		return found != index[ruleSet].end() ? std::optional<RuleSlot>{ found->second } : std::nullopt;
	}

	void traceSidDecision(
		std::string_view stage,
		std::uint32_t formID,
		const RuleTranslation& entry,
		std::optional<std::uint32_t> decodedSid,
		const RuleSlot* target)
	{
		if (!g_traceEnabled.load(std::memory_order_relaxed))
		{
			return;
		}
		if (g_traceLines.fetch_add(1, std::memory_order_relaxed) >= kTraceLimit)
		{
			return;
		}

		REX::INFO(
			"{} innr-translate trace stage={} form={:08X} sid={:08X} decoded={}:{} decodedSid={:08X} target={}:{} textLen={}",
			Plugin::NAME,
			stage,
			formID,
			entry.sid,
			entry.ruleSet,
			entry.ruleOffset,
			decodedSid.value_or(0),
			target ? target->ruleSet : 0,
			target ? target->ruleOffset : 0,
			entry.text.size());
	}

	[[nodiscard]] RE::BGSInstanceNamingRules::RuleData* resolveRuleByVerifiedStringID(
		RE::BGSInstanceNamingRules* rules,
		const RuleConditionIndex* conditionIndex,
		std::uint32_t formID,
		const RuleTranslation& entry,
		ApplyTotals& totals)
	{
		auto* rule = ruleAtSlot(rules, entry.ruleSet, entry.ruleOffset);
		if (entry.usesConditionKey)
		{
			if (conditionIndex)
			{
				if (const auto slot = findRuleByConditionHash(*conditionIndex, entry.ruleSet, entry.conditionHash))
				{
					return slot->rule;
				}
			}
			else if (const auto slot = findRuleByStringID(rules, entry.sid))
			{
				return slot->rule;
			}
			++totals.skippedSlotOutOfRange;
			return nullptr;
		}
		if (!entry.sid)
		{
			return rule;
		}
		if (!rule)
		{
			if (const auto slot = findRuleByStringID(rules, entry.sid))
			{
				++totals.sidRemappedRules;
				traceSidDecision("sid-fallback", formID, entry, std::nullopt, std::addressof(*slot));
				return slot->rule;
			}
			++totals.skippedSlotOutOfRange;
			return nullptr;
		}

		const auto decodedSid = readRuleStringID(*rule);
		if (!decodedSid || *decodedSid == entry.sid)
		{
			return rule;
		}
		if (const auto slot = findRuleByStringID(rules, entry.sid))
		{
			++totals.sidRemappedRules;
			traceSidDecision("sid-remap", formID, entry, decodedSid, std::addressof(*slot));
			return slot->rule;
		}

		++totals.skippedStringIDMismatch;
		traceSidDecision("sid-mismatch", formID, entry, decodedSid, nullptr);
		return nullptr;
	}

	void traceFormApply(std::uint32_t formID, const ApplyTotals& totals, std::size_t available)
	{
		if (!g_traceEnabled.load(std::memory_order_relaxed))
		{
			return;
		}
		if (g_traceLines.fetch_add(1, std::memory_order_relaxed) >= kTraceLimit)
		{
			return;
		}

		REX::INFO(
			"{} innr-translate trace stage=data-apply form={:08X} available={} applied={} unchanged={} missingRuntimeForm={} slotOutOfRange={} sidRemap={} sidMismatch={}",
			Plugin::NAME,
			formID,
			available,
			totals.appliedRules,
			totals.unchangedRules,
			totals.skippedRuntimeForm,
			totals.skippedSlotOutOfRange,
			totals.sidRemappedRules,
			totals.skippedStringIDMismatch);
	}

	[[nodiscard]] ApplyTotals applyFormRules(
		std::uint32_t formID,
		const std::vector<RuleTranslation>& entries)
	{
		ApplyTotals totals;
		auto* rules = RE::TESForm::GetFormByID<RE::BGSInstanceNamingRules>(formID);
		if (!rules)
		{
			totals.skippedRuntimeForm = entries.size();
			traceFormApply(formID, totals, entries.size());
			return totals;
		}

		bool needsConditionIndex = false;
		for (const auto& entry : entries)
		{
			needsConditionIndex = needsConditionIndex || entry.usesConditionKey;
		}
		std::optional<RuleConditionIndex> conditionIndex;
		if (needsConditionIndex)
		{
			conditionIndex.emplace(buildRuleConditionIndex(rules));
		}
		for (const auto& entry : entries)
		{
			auto* rule = resolveRuleByVerifiedStringID(rules, conditionIndex ? std::addressof(*conditionIndex) : nullptr, formID, entry, totals);
			if (!rule)
			{
				continue;
			}

			RuntimeTextStringAssign::AssignPlainLocalized(rule->text, entry.text);
			++totals.appliedRules;
		}

		traceFormApply(formID, totals, entries.size());
		return totals;
	}

	[[nodiscard]] ApplyTotals applyAllLocked()
	{
		ApplyTotals totals;
		for (const auto& [formID, entries] : g_rulesByForm)
		{
			const auto formTotals = applyFormRules(formID, entries);
			totals.appliedRules += formTotals.appliedRules;
			totals.unchangedRules += formTotals.unchangedRules;
			totals.skippedRuntimeForm += formTotals.skippedRuntimeForm;
			totals.skippedSlotOutOfRange += formTotals.skippedSlotOutOfRange;
			totals.sidRemappedRules += formTotals.sidRemappedRules;
			totals.skippedStringIDMismatch += formTotals.skippedStringIDMismatch;
		}
		return totals;
	}

	void copyApplyTotals(RuntimeInstanceNamingTranslations::RebuildStats& stats, const ApplyTotals& totals)
	{
		stats.appliedRules = totals.appliedRules;
		stats.unchangedRules = totals.unchangedRules;
		stats.skippedRuntimeForm = totals.skippedRuntimeForm;
		stats.skippedSlotOutOfRange = totals.skippedSlotOutOfRange;
		stats.sidRemappedRules = totals.sidRemappedRules;
		stats.skippedStringIDMismatch = totals.skippedStringIDMismatch;
	}

	void logStats(std::string_view stage, const RuntimeInstanceNamingTranslations::RebuildStats& stats, std::size_t forms)
	{
		if (!RuntimeApplySettings::Load().TraceEnabled())
		{
			return;
		}

		REX::INFO(
			"{} innr-translate {}: accepted={} forms={} applied={} unchanged={} missingForm={} missingRuntimeForm={} missingSlot={} emptyText={} nonINNR={} slotOutOfRange={} sidRemap={} sidMismatch={}.",
			Plugin::NAME,
			stage,
			stats.accepted,
			forms,
			stats.appliedRules,
			stats.unchangedRules,
			stats.skippedMissingForm,
			stats.skippedRuntimeForm,
			stats.skippedMissingIndex,
			stats.skippedEmptyText,
			stats.skippedWrongType,
			stats.skippedSlotOutOfRange,
			stats.sidRemappedRules,
			stats.skippedStringIDMismatch);
	}
}

namespace RuntimeInstanceNamingTranslations
{
	RebuildStats Rebuild(const TranslationCatalogBuildResult& catalog)
	{
		const auto settings = RuntimeApplySettings::Load();
		g_traceEnabled.store(settings.TraceEnabled(), std::memory_order_relaxed);

		{
			std::scoped_lock lock{ g_lock };
			if (g_rebuiltCatalog == std::addressof(catalog) && g_ready.load(std::memory_order_acquire))
			{
				g_traceLines.store(0, std::memory_order_relaxed);
				auto stats = g_lastRebuildStats;
				stats.appliedRules = 0;
				stats.unchangedRules = 0;
				stats.skippedRuntimeForm = 0;
				stats.skippedSlotOutOfRange = 0;
				stats.sidRemappedRules = 0;
				stats.skippedStringIDMismatch = 0;
				logStats("data cached", stats, g_rulesByForm.size());
				return stats;
			}
		}

		std::unordered_map<std::uint32_t, std::vector<RuleTranslation>> rulesByForm;
		RebuildStats stats;

		for (const auto& record : catalog.records)
		{
			if (!isInstanceNamingRecord(record))
			{
				++stats.skippedWrongType;
				continue;
			}
			if (record.data.replacerText.empty())
			{
				++stats.skippedEmptyText;
				continue;
			}
			if (!record.data.index)
			{
				++stats.skippedMissingIndex;
				continue;
			}

			std::uint32_t ruleSet = 0;
			std::uint32_t ruleOffset = 0;
			std::uint32_t conditionHash = 0;
			bool usesConditionKey = false;
			if (PluginEdidIndex::DecodeInstanceNamingRuleConditionKey(*record.data.index, ruleSet, conditionHash))
			{
				usesConditionKey = true;
			}
			else if (!PluginEdidIndex::DecodeInstanceNamingRuleSlot(*record.data.index, ruleSet, ruleOffset))
			{
				++stats.skippedMissingIndex;
				continue;
			}
			if (ruleSet >= 10)
			{
				++stats.skippedMissingIndex;
				continue;
			}

			auto* form = resolveForm(record);
			auto* rules = form ? form->As<RE::BGSInstanceNamingRules>() : nullptr;
			if (!rules)
			{
				++stats.skippedMissingForm;
				continue;
			}

			rulesByForm[rules->formID].push_back(RuleTranslation{
				.ruleSet = ruleSet,
				.ruleOffset = ruleOffset,
				.conditionHash = conditionHash,
				.sid = record.data.stringID.value_or(0),
				.text = record.data.replacerText,
				.usesConditionKey = usesConditionKey
			});
			++stats.accepted;
		}

		std::size_t formCount = 0;
		{
			std::scoped_lock lock{ g_lock };
			g_rulesByForm = std::move(rulesByForm);
			g_rebuiltCatalog = std::addressof(catalog);
			g_traceLines.store(0, std::memory_order_relaxed);
			copyApplyTotals(stats, applyAllLocked());
			g_lastRebuildStats = stats;
			formCount = g_rulesByForm.size();
		}
		g_ready.store(true, std::memory_order_release);

		logStats("data map rebuilt", stats, formCount);
		return stats;
	}

	RebuildStats Reapply(const TranslationCatalogBuildResult& catalog)
	{
		const auto settings = RuntimeApplySettings::Load();
		g_traceEnabled.store(settings.TraceEnabled(), std::memory_order_relaxed);

		{
			std::scoped_lock lock{ g_lock };
			if (g_rebuiltCatalog == std::addressof(catalog) && g_ready.load(std::memory_order_acquire))
			{
				g_traceLines.store(0, std::memory_order_relaxed);
				auto stats = g_lastRebuildStats;
				copyApplyTotals(stats, applyAllLocked());
				g_lastRebuildStats = stats;
				logStats("data reapplied", stats, g_rulesByForm.size());
				return stats;
			}
		}

		return Rebuild(catalog);
	}
}
