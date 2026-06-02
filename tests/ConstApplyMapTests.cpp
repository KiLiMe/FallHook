// AI CONTEXT: Tests source-free const apply map construction.
// Depends on ConstApplyMap and catalog data types only.
// Runtime scope is Fallout 4 1.10.163 non-hook apply categories without loading the game.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: test records contain target identity and destination text only.
#include "ConstApplyMap.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace
{
	void requireConstApply(bool condition, std::string_view message)
	{
		if (!condition)
		{
			std::cerr << message << '\n';
			std::exit(1);
		}
	}

	TranslationCatalogRecord record(
		TranslationType type,
		std::string_view text,
		std::optional<std::uint32_t> formID,
		std::optional<std::string> editorID,
		std::string_view signature,
		std::optional<std::uint32_t> index = std::nullopt)
	{
		SourceFreeTranslationData data;
		data.translationType = type;
		data.replacerText = std::string{ text };
		data.formID = formID;
		data.editorID = std::move(editorID);
		data.index = index;

		return TranslationCatalogRecord{
			.key = {},
			.data = std::move(data),
			.pluginName = "example.esp",
			.recordSignature = std::string{ signature }
		};
	}
}

void testConstApplyMap()
{
	TranslationCatalogBuildResult catalog;
	catalog.records.push_back(record(TranslationType::kFullName, "Name", 0x01001234, std::nullopt, "WEAP FULL"));
	catalog.records.push_back(record(TranslationType::kFullName, "Fragment", 0x01001234, std::nullopt, "WEAP FULL", 2));
	catalog.records.push_back(record(TranslationType::kQuestObjective, "Objective", std::nullopt, "QuestA", "QUST NNAM"));
	catalog.records.push_back(record(TranslationType::kGameSetting, "Setting", std::nullopt, "sName", "GMST DATA"));
	catalog.records.push_back(record(TranslationType::kRuntime1, "Deferred", 0x01004567, std::nullopt, "BOOK DESC"));
	catalog.records.push_back(record(TranslationType::kMessageShortName, "", 0x01009999, std::nullopt, "MESG NNAM"));
	catalog.records.push_back(record(TranslationType::kNpcFullName, "Ghoul", 0x01007777, std::nullopt, "NPC_ FULL"));
	catalog.records.push_back(record(TranslationType::kNpcFullName, "SkinBase", 0x01007777, std::nullopt, "NPC_ FULL", 0));
	catalog.records.push_back(record(TranslationType::kActorValueAbbreviation, "STR", 0x000002C2, "Strength", "AVIF ANAM"));
	catalog.records.push_back(record(TranslationType::kActorValueAbbreviation, "Buc xa", 0x000002EB, "Rads", "AVIF ANAM"));
	catalog.records.push_back(record(TranslationType::kActorValueAbbreviation, "Khang sat thuong", 0x000002DA, "DamageResist", "AVIF ANAM"));
	catalog.records.push_back(record(TranslationType::kActorValueAbbreviation, "Poison", 0x000002E4, "PoisonResist", "AVIF ANAM"));
	catalog.records.push_back(record(TranslationType::kActorValueAbbreviation, "Wrong route", 0x000002DA, "DamageResist", "AVIF FULL"));
	catalog.records.push_back(record(TranslationType::kRegion, "The Fens", 0x01002222, "MapDistrictFens", "REGN RDMP"));

	const auto maps = ConstApplyMap::Build(catalog);
	requireConstApply(maps.stats.totalRecords == 14, "const map total count mismatch");
	requireConstApply(maps.stats.formEntries == 6, "const map form count mismatch");
	requireConstApply(maps.stats.editorIDFormEntries == 1, "const map editor form count mismatch");
	requireConstApply(maps.stats.gameSettingEntries == 1, "const map game setting count mismatch");
	requireConstApply(maps.stats.skippedUnsupportedType == 5, "const map unsupported count mismatch");
	requireConstApply(maps.stats.skippedEmptyText == 1, "const map empty text count mismatch");
	requireConstApply(maps.gameSettingEntries.contains("sname"), "const map normalized game setting key missing");
	requireConstApply(ConstApplyMap::IsDirectConstApplyType(TranslationType::kButtonText1), "MESG:ITXT should be direct const apply");
	requireConstApply(ConstApplyMap::IsDirectConstApplyType(TranslationType::kActorValueAbbreviation), "AVIF:ANAM should be direct const apply");
	requireConstApply(ConstApplyMap::IsDirectConstApplyType(TranslationType::kRegion), "REGN:RDMP should be direct const apply");
	requireConstApply(!ConstApplyMap::IsDirectConstApplyType(TranslationType::kNpcFullName), "NPC_:FULL should stay pre-data only");
	requireConstApply(!ConstApplyMap::IsDirectConstApplyType(TranslationType::kInstanceNamePart), "INNR:WNAM should wait for data slot apply");
	requireConstApply(!ConstApplyMap::IsDirectConstApplyType(TranslationType::kRuntimeIndex), "INFO:NAM1 should wait for dialogue data apply");
}
