// AI CONTEXT: Builds Pip-Boy log GMST label maps from source-free catalog records.
// Depends on RuntimeApplySettings for gated diagnostics and TranslationCatalog data.
// Runtime scope is Fallout 4 1.11.191 GMST:DATA records used by Pip-Boy log stats.
// Version-specific logic: Fallout 4 1.11.191 only; no alternate runtime branches.
// Source-free policy: runtime keys resolve to GMST editor IDs; Source text is never stored.
#include "PCH.h"

#include "RuntimeApplySettings.h"
#include "111191/RuntimePipboyLogTranslations.h"

#include <cctype>
#include <mutex>
#include <unordered_map>

namespace Runtime111191
{
namespace
{
	std::mutex g_lock;
	std::unordered_map<std::string, std::string> g_byEditorID;
	std::unordered_map<std::string, std::string> g_aliasEditorID;

	std::string lower(std::string_view value)
	{
		std::string result{ value };
		std::ranges::transform(result, result.begin(), [](unsigned char ch) {
			return static_cast<char>(std::tolower(ch));
		});
		return result;
	}

	std::string trim(std::string_view value)
	{
		const auto begin = value.find_first_not_of(" \t\r\n");
		if (begin == std::string_view::npos)
		{
			return {};
		}
		const auto end = value.find_last_not_of(" \t\r\n");
		return std::string{ value.substr(begin, end - begin + 1) };
	}

	bool hasText(std::string_view text)
	{
		return std::ranges::any_of(text, [](unsigned char ch) {
			return !std::isspace(ch);
		});
	}

	bool startsWithIgnoreCase(std::string_view value, std::string_view prefix)
	{
		return value.size() >= prefix.size() && lower(value.substr(0, prefix.size())) == lower(prefix);
	}

	std::string compactStatKey(std::string_view key)
	{
		auto value = trim(key);
		if (!value.empty() && value.front() == '$')
		{
			value = trim(std::string_view{ value }.substr(1));
		}

		std::string compact;
		compact.reserve(value.size());
		for (const auto ch : value)
		{
			if (std::isalnum(static_cast<unsigned char>(ch)) != 0)
			{
				compact.push_back(ch);
			}
		}
		return compact;
	}

	void addAlias(std::string_view key, std::string_view editorID)
	{
		const auto compact = compactStatKey(key);
		if (!compact.empty())
		{
			g_aliasEditorID.insert_or_assign(lower(compact), std::string{ editorID });
		}
	}

	void addStaticAliases()
	{
		struct Alias
		{
			std::string_view key;
			std::string_view editorID;
		};

		static constexpr std::array aliases{
			Alias{ "DaysPassed"sv, "sMiscStatDaysPassed"sv },
			Alias{ "LocationsDiscovered"sv, "sMiscStatLocationsDiscovered"sv },
			Alias{ "LocationsCleared"sv, "sMiscStatDungeonsCleared"sv },
			Alias{ "DungeonsCleared"sv, "sMiscStatDungeonsCleared"sv },
			Alias{ "FusionCoresUsed"sv, "sMiscStatFusionCoresConsumed"sv },
			Alias{ "WeaponModsCrafted"sv, "sMiscStatWeaponsImproved"sv },
			Alias{ "WeaponsImproved"sv, "sMiscStatWeaponsImproved"sv },
			Alias{ "RobotsHacked"sv, "sMiscStatRobotsDisabled"sv },
			Alias{ "RobotsDisabled"sv, "sMiscStatRobotsDisabled"sv },
			Alias{ "ArmorModsCrafted"sv, "sMiscStatArmorImproved"sv },
			Alias{ "ArmorImproved"sv, "sMiscStatArmorImproved"sv },
			Alias{ "MagazinesFound"sv, "sMiscStatSkillBooksRead"sv },
			Alias{ "SkillBooksRead"sv, "sMiscStatSkillBooksRead"sv },
			Alias{ "SupplyLinesCreated"sv, "sMiscStatSuppyLinesCreated"sv },
			Alias{ "SuppyLinesCreated"sv, "sMiscStatSuppyLinesCreated"sv },
			Alias{ "BrotherhoodofSteelQuestsComp"sv, "sMiscStatBoSQuestsCompleted"sv },
			Alias{ "BrotherhoodofSteelQuestsCompleted"sv, "sMiscStatBoSQuestsCompleted"sv },
			Alias{ "BrotherhoodOfSteelQuestsCompleted"sv, "sMiscStatBoSQuestsCompleted"sv },
			Alias{ "BoSQuestsCompleted"sv, "sMiscStatBoSQuestsCompleted"sv },
			Alias{ "AutomatronQuestsCompleted"sv, "sMiscStatDLC01QuestsCompleted"sv },
			Alias{ "DLC01QuestsCompleted"sv, "sMiscStatDLC01QuestsCompleted"sv },
			Alias{ "InstituteQuestsCompleted"sv, "sMiscStatInstQuestsCompleted"sv },
			Alias{ "InstQuestsCompleted"sv, "sMiscStatInstQuestsCompleted"sv },
			Alias{ "RailroadQuestsCompleted"sv, "sMiscStatRRQuestsCompleted"sv },
			Alias{ "RRQuestsCompleted"sv, "sMiscStatRRQuestsCompleted"sv },
			Alias{ "MinutemenQuestsCompleted"sv, "sMiscStatMMQuestsCompleted"sv },
			Alias{ "MMQuestsCompleted"sv, "sMiscStatMMQuestsCompleted"sv }
		};

		for (const auto& alias : aliases)
		{
			addAlias(alias.key, alias.editorID);
		}
	}

	void addDerivedAlias(std::string_view editorID)
	{
		for (const auto prefix : { "sMiscStat"sv, "sStats"sv, "sPipboy"sv })
		{
			if (startsWithIgnoreCase(editorID, prefix) && editorID.size() > prefix.size())
			{
				addAlias(editorID.substr(prefix.size()), editorID);
				return;
			}
		}
	}

	std::optional<std::string_view> aliasEditorID(std::string_view compact)
	{
		const auto found = g_aliasEditorID.find(lower(compact));
		if (found == g_aliasEditorID.end())
		{
			return std::nullopt;
		}
		return found->second;
	}

	std::optional<RuntimePipboyLogTranslations::LookupResult> lookupEditorID(std::string_view editorID)
	{
		const auto it = g_byEditorID.find(lower(editorID));
		if (it == g_byEditorID.end())
		{
			return std::nullopt;
		}
		return RuntimePipboyLogTranslations::LookupResult{ std::string{ editorID }, it->second.c_str() };
	}
}

namespace RuntimePipboyLogTranslations
{
	void Rebuild(const TranslationCatalogBuildResult& catalog)
	{
		std::scoped_lock lock{ g_lock };
		BuildStats stats;
		stats.catalogRecords = catalog.records.size();
		g_byEditorID.clear();
		g_aliasEditorID.clear();
		addStaticAliases();

		for (const auto& record : catalog.records)
		{
			const auto& data = record.data;
			if (data.translationType != TranslationType::kGameSetting || record.recordSignature != "GMST DATA")
			{
				++stats.skippedWrongType;
				continue;
			}
			if (!data.editorID || data.editorID->empty())
			{
				++stats.skippedMissingEditorID;
				continue;
			}
			if (!hasText(data.replacerText))
			{
				++stats.skippedEmptyText;
				continue;
			}

			g_byEditorID.insert_or_assign(lower(*data.editorID), data.replacerText);
			addDerivedAlias(*data.editorID);
			++stats.accepted;
		}

		if (RuntimeApplySettings::Load().TraceEnabled())
		{
			REX::INFO(
				"{} pipboy-log GMST map built: accepted={} skippedMissingEditorID={} skippedEmptyText={}.",
				Plugin::NAME,
				stats.accepted,
				stats.skippedMissingEditorID,
				stats.skippedEmptyText);
		}
	}

	std::optional<LookupResult> LookupStatKey(std::string_view key)
	{
		std::scoped_lock lock{ g_lock };
		auto text = trim(key);
		if (text.empty())
		{
			return std::nullopt;
		}
		if (!text.empty() && text.front() == '$')
		{
			text = trim(std::string_view{ text }.substr(1));
		}

		if (startsWithIgnoreCase(text, "sMiscStat"sv) ||
			startsWithIgnoreCase(text, "sStats"sv) ||
			startsWithIgnoreCase(text, "sPipboy"sv))
		{
			if (auto found = lookupEditorID(text))
			{
				return found;
			}
		}

		const auto compact = compactStatKey(text);
		if (compact.empty())
		{
			return std::nullopt;
		}

		for (const auto& candidate : { std::format("sMiscStat{}", compact), std::format("sStats{}", compact) })
		{
			if (auto found = lookupEditorID(candidate))
			{
				return found;
			}
		}
		if (auto alias = aliasEditorID(compact))
		{
			return lookupEditorID(*alias);
		}
		return std::nullopt;
	}
}

} // namespace Runtime111191
