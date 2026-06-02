// AI CONTEXT: Builds source-free PERK:EPF2 activate-choice lookup maps.
// Depends on RuntimeFormResolver and TranslationCatalog records.
// Runtime scope is Fallout 4 1.10.163 perk entry-point activate-choice data.
// Version-specific logic: none; HUDRollover owns runtime hook addresses.
// Source-free policy: stores destination text keyed by form/index/sID, never by Source text.
#include "PCH.h"

#include "110163/RuntimePerkActivateChoiceTranslations.h"

#include "RuntimeApplySettings.h"
#include "SourceFreeTranslationKey.h"
#include "110163/RuntimeFormResolver.h"

#include "RE/T/TESForm.h"

#include <mutex>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace
{
	std::mutex g_lock;
	const TranslationCatalogBuildResult* g_rebuiltCatalog{ nullptr };
	std::unordered_map<std::uint64_t, std::string> g_byFormIndex;
	std::unordered_map<std::uint64_t, std::string> g_byFormSid;
	std::unordered_map<std::string, std::string> g_byEditorTarget;
	std::unordered_map<std::uint32_t, std::string> g_byUniqueSid;
	std::unordered_set<std::string> g_ambiguousEditorTarget;
	std::unordered_set<std::uint32_t> g_ambiguousSid;
	std::optional<std::string> g_defaultText;
	std::optional<std::string> g_roboticsExpertHackText;
	RuntimePerkActivateChoiceTranslations::RebuildStats g_lastStats;
	constexpr std::uint32_t kRoboticsExpertHackSid{ 0x0314B5 };
	constexpr std::string_view kRoboticsExpertEditorID{ "RoboticsExpert01" };

	[[nodiscard]] bool isPerkActivateChoiceRecord(const TranslationCatalogRecord& record)
	{
		return record.data.translationType == TranslationType::kButtonText2 &&
			record.recordSignature == "PERK EPF2";
	}

	[[nodiscard]] bool isRoboticsExpertHackRecord(const TranslationCatalogRecord& record)
	{
		if (!isPerkActivateChoiceRecord(record) || record.data.stringID != kRoboticsExpertHackSid)
		{
			return false;
		}
		return !record.data.editorID || *record.data.editorID == kRoboticsExpertEditorID;
	}

	[[nodiscard]] std::uint64_t formSlotKey(std::uint32_t formID, std::uint32_t slot) noexcept
	{
		return (static_cast<std::uint64_t>(formID) << 32) | slot;
	}

	[[nodiscard]] std::optional<std::uint32_t> resolveRuntimeFormID(const TranslationCatalogRecord& record)
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

	void addSidFallback(std::uint32_t stringID, std::string_view text)
	{
		if (g_ambiguousSid.contains(stringID))
		{
			return;
		}

		const auto [it, inserted] = g_byUniqueSid.emplace(stringID, text);
		if (!inserted && it->second != text)
		{
			g_byUniqueSid.erase(stringID);
			g_ambiguousSid.insert(stringID);
		}
	}

	void addEditorTarget(std::string_view editorID, std::string_view text)
	{
		const auto key = SourceFreeTranslationKeys::NormalizeEditorID(editorID);
		if (key.empty() || g_ambiguousEditorTarget.contains(key))
		{
			return;
		}

		const auto [it, inserted] = g_byEditorTarget.emplace(key, text);
		if (!inserted && it->second != text)
		{
			g_byEditorTarget.erase(it);
			g_ambiguousEditorTarget.insert(key);
		}
	}
}

namespace RuntimePerkActivateChoiceTranslations
{
	RebuildStats Rebuild(const TranslationCatalogBuildResult& catalog, bool force)
	{
		std::scoped_lock lock{ g_lock };
		if (!force && g_rebuiltCatalog == std::addressof(catalog))
		{
			return g_lastStats;
		}

		g_byFormIndex.clear();
		g_byFormSid.clear();
		g_byEditorTarget.clear();
		g_byUniqueSid.clear();
		g_ambiguousEditorTarget.clear();
		g_ambiguousSid.clear();
		g_roboticsExpertHackText.reset();
		std::unordered_set<std::string> uniqueText;
		RebuildStats stats;

		for (const auto& record : catalog.records)
		{
			if (!isPerkActivateChoiceRecord(record))
			{
				++stats.skippedWrongType;
				continue;
			}
			if (record.data.replacerText.empty())
			{
				++stats.skippedEmptyText;
				continue;
			}

			bool accepted = false;
			const auto runtimeFormID = resolveRuntimeFormID(record);
			if (record.data.formID && !runtimeFormID)
			{
				++stats.skippedUnresolvedFormID;
			}
			if (runtimeFormID && record.data.index)
			{
				g_byFormIndex.insert_or_assign(
					formSlotKey(*runtimeFormID, *record.data.index),
					record.data.replacerText);
				++stats.formIndexEntries;
				accepted = true;
			}
			if (runtimeFormID && record.data.stringID)
			{
				g_byFormSid.insert_or_assign(
					formSlotKey(*runtimeFormID, *record.data.stringID),
					record.data.replacerText);
				++stats.formSidEntries;
				accepted = true;
			}
			if (record.data.stringID)
			{
				addSidFallback(*record.data.stringID, record.data.replacerText);
				accepted = true;
			}
			if (record.data.editorID && !record.data.editorID->empty())
			{
				addEditorTarget(*record.data.editorID, record.data.replacerText);
				accepted = true;
			}

			if (accepted)
			{
				++stats.accepted;
				uniqueText.insert(record.data.replacerText);
				if (isRoboticsExpertHackRecord(record))
				{
					g_roboticsExpertHackText = record.data.replacerText;
					++stats.roboticsExpertHackEntries;
				}
			}
			else
			{
				++stats.skippedMissingTarget;
			}
		}

		stats.editorTargetEntries = g_byEditorTarget.size();
		stats.ambiguousEditorTargets = g_ambiguousEditorTarget.size();
		stats.uniqueSidEntries = g_byUniqueSid.size();
		stats.ambiguousSidEntries = g_ambiguousSid.size();
		g_defaultText = uniqueText.size() == 1 ? std::optional<std::string>{ *uniqueText.begin() } : std::nullopt;
		g_rebuiltCatalog = std::addressof(catalog);
		g_lastStats = stats;

		if (RuntimeApplySettings::Load().TraceEnabled())
		{
			REX::INFO(
				"{} perk-activate-choice map built: accepted={} formIndex={} formSid={} editorTargets={} ambiguousEditor={} uniqueSid={} ambiguousSid={} unresolvedFormID={} emptyText={}.",
				Plugin::NAME,
				stats.accepted,
				stats.formIndexEntries,
				stats.formSidEntries,
				stats.editorTargetEntries,
				stats.ambiguousEditorTargets,
				stats.uniqueSidEntries,
				stats.ambiguousSidEntries,
				stats.skippedUnresolvedFormID,
				stats.skippedEmptyText);
			REX::INFO(
				"{} perk-activate-choice special map built: roboticsExpertHackEntries={} roboticsExpertHackLen={}.",
				Plugin::NAME,
				stats.roboticsExpertHackEntries,
				g_roboticsExpertHackText ? g_roboticsExpertHackText->size() : 0);
		}
		return stats;
	}

	std::optional<std::string> Lookup(
		const RE::TESForm* perk,
		std::optional<std::uint32_t> index,
		std::optional<std::uint32_t> stringID)
	{
		std::scoped_lock lock{ g_lock };
		if (perk && index)
		{
			if (const auto it = g_byFormIndex.find(formSlotKey(perk->formID, *index)); it != g_byFormIndex.end())
			{
				return it->second;
			}
		}
		if (perk && stringID)
		{
			if (const auto it = g_byFormSid.find(formSlotKey(perk->formID, *stringID)); it != g_byFormSid.end())
			{
				return it->second;
			}
		}
		if (stringID)
		{
			if (const auto it = g_byUniqueSid.find(*stringID); it != g_byUniqueSid.end())
			{
				return it->second;
			}
		}
		return std::nullopt;
	}

	std::optional<std::string> LookupHudSecondaryTarget(std::string_view editorID)
	{
		std::scoped_lock lock{ g_lock };
		const auto key = SourceFreeTranslationKeys::NormalizeEditorID(editorID);
		if (key.empty() || g_ambiguousEditorTarget.contains(key))
		{
			return std::nullopt;
		}
		if (const auto it = g_byEditorTarget.find(key); it != g_byEditorTarget.end())
		{
			return it->second;
		}
		return std::nullopt;
	}

	std::optional<std::string> DefaultText()
	{
		std::scoped_lock lock{ g_lock };
		return g_defaultText;
	}

	std::optional<std::string> RoboticsExpertHackText()
	{
		std::scoped_lock lock{ g_lock };
		return g_roboticsExpertHackText;
	}
}
