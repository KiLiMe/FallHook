// AI CONTEXT: Builds source-free PERK:EPF2 HUD secondary-action lookup maps.
// Depends on normalized editor identity and the verified RoboticsExpert HACK sID.
// Runtime scope is Fallout 4 1.10.163 perk entry-point activate-choice data.
// Version-specific logic: none; HUDRollover owns runtime hook addresses.
// Source-free policy: stores destination text keyed by editor identity or verified special sID, never by Source text.
#include "PCH.h"

#include "110163/RuntimePerkActivateChoiceTranslations.h"

#include "RuntimeApplySettings.h"
#include "SourceFreeTranslationKey.h"

#include <mutex>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace
{
	std::mutex g_lock;
	const TranslationCatalogBuildResult* g_rebuiltCatalog{ nullptr };
	std::unordered_map<std::string, std::string> g_byEditorTarget;
	std::unordered_set<std::string> g_ambiguousEditorTarget;
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

		g_byEditorTarget.clear();
		g_ambiguousEditorTarget.clear();
		g_roboticsExpertHackText.reset();
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
			if (record.data.editorID && !record.data.editorID->empty())
			{
				addEditorTarget(*record.data.editorID, record.data.replacerText);
				accepted = true;
			}
			if (isRoboticsExpertHackRecord(record))
			{
				g_roboticsExpertHackText = record.data.replacerText;
				++stats.roboticsExpertHackEntries;
				accepted = true;
			}

			if (accepted)
			{
				++stats.accepted;
			}
			else
			{
				++stats.skippedMissingTarget;
			}
		}

		stats.editorTargetEntries = g_byEditorTarget.size();
		stats.ambiguousEditorTargets = g_ambiguousEditorTarget.size();
		g_rebuiltCatalog = std::addressof(catalog);
		g_lastStats = stats;

		if (RuntimeApplySettings::Load().TraceEnabled())
		{
			REX::INFO(
				"{} perk-activate-choice map built: accepted={} editorTargets={} ambiguousEditor={} emptyText={}.",
				Plugin::NAME,
				stats.accepted,
				stats.editorTargetEntries,
				stats.ambiguousEditorTargets,
				stats.skippedEmptyText);
			REX::INFO(
				"{} perk-activate-choice special map built: roboticsExpertHackEntries={} roboticsExpertHackLen={}.",
				Plugin::NAME,
				stats.roboticsExpertHackEntries,
				g_roboticsExpertHackText ? g_roboticsExpertHackText->size() : 0);
		}
		return stats;
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

	std::optional<std::string> RoboticsExpertHackText()
	{
		std::scoped_lock lock{ g_lock };
		return g_roboticsExpertHackText;
	}
}
