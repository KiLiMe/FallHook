// AI CONTEXT: Tests source-free catalog construction and cached pipeline loading.
// Depends on catalog, pipeline, plugin EDID index, and shared binary fixture helpers.
// Runtime scope is Fallout 4 1.10.163 data-file semantics without loading the game.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: verifies catalog keys are stable identity keys, not source text.
#include "FallHookTestSupport.h"
#include "PluginEdidIndex.h"
#include "TranslationCatalog.h"
#include "TranslationPipeline.h"
#include "TranslationPreparedData.h"
#include "XmlLoadOrder.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>

namespace
{
	std::vector<std::uint8_t> bodyPartData(std::uint8_t type)
	{
		std::vector<std::uint8_t> data(101, 0);
		data[0] = 0x00;
		data[1] = 0x00;
		data[2] = 0x80;
		data[3] = 0x3F;
		data[65] = type;
		return data;
	}

	XmlTranslationFile makeCatalogXml(std::string_view pathName, std::string_view dest, std::string_view source)
	{
		XmlTranslationEntry entry;
		entry.edid = "[01001234]";
		entry.record = "QUST:NNAM";
		entry.source = std::string{ source };
		entry.dest = std::string{ dest };
		entry.index = 2;

		XmlTranslationFile file;
		file.path = std::filesystem::path{ pathName };
		file.addon = "Example.esp";
		file.entries.push_back(std::move(entry));
		return file;
	}
}

void testTranslationCatalog()
{
	const std::vector<TranslationCatalogFile> pluginFiles{
		{ makeCatalogXml("low.xml", "Low priority", "A"), 1 },
		{ makeCatalogXml("high.xml", "High priority", "B"), 10 }
	};
	const auto pluginCatalog = TranslationCatalog::Build(pluginFiles, XmlLoadOrder::Mode::kPlugin);
	FallHookTestSupport::require(pluginCatalog.totalEntries == 2, "catalog total count mismatch");
	FallHookTestSupport::require(pluginCatalog.acceptedEntries == 1, "catalog accepted count mismatch");
	FallHookTestSupport::require(pluginCatalog.records.size() == 1, "catalog duplicate key collapse mismatch");
	FallHookTestSupport::require(pluginCatalog.records[0].data.replacerText == "High priority", "plugin priority winner mismatch");

	const std::vector<TranslationCatalogFile> filenameFiles{
		{ makeCatalogXml("a.xml", "First file", "Original A"), 0 },
		{ makeCatalogXml("b.xml", "Second file", "Original B"), 0 }
	};
	const auto filenameCatalog = TranslationCatalog::Build(filenameFiles, XmlLoadOrder::Mode::kFilename);
	FallHookTestSupport::require(filenameCatalog.records[0].data.replacerText == "Second file", "filename mode overwrite mismatch");
	FallHookTestSupport::require(filenameCatalog.overwrittenEntries == 1, "filename overwrite count mismatch");
	FallHookTestSupport::require(filenameCatalog.records[0].key == pluginCatalog.records[0].key, "catalog key changed with source payload");

	XmlTranslationEntry sourceIgnoredEntry;
	sourceIgnoredEntry.record = "GMST:DATA";
	sourceIgnoredEntry.stringID = 0x00035A2E;
	sourceIgnoredEntry.source = "[FAILED] Source text should not matter";
	sourceIgnoredEntry.dest = "[WRONG] Destination must be preserved";

	XmlTranslationEntry blankSourceEntry;
	blankSourceEntry.record = "GMST:DATA";
	blankSourceEntry.stringID = 0x00035A2F;
	blankSourceEntry.dest = "Blank source works";

	XmlTranslationFile sourceIgnoredFile;
	sourceIgnoredFile.path = "source_ignored.xml";
	sourceIgnoredFile.addon = "Fallout4.esm";
	sourceIgnoredFile.entries.push_back(std::move(sourceIgnoredEntry));
	sourceIgnoredFile.entries.push_back(std::move(blankSourceEntry));

	const std::array sourceIgnoredFiles{ TranslationCatalogFile{ sourceIgnoredFile, 0, nullptr } };
	const auto sourceIgnoredCatalog = TranslationCatalog::Build(sourceIgnoredFiles, XmlLoadOrder::Mode::kPlugin);
	FallHookTestSupport::require(sourceIgnoredCatalog.records.size() == 2, "sID-only catalog entries missing");
	FallHookTestSupport::require(sourceIgnoredCatalog.records[0].data.replacerText == "[WRONG] Destination must be preserved", "catalog used Source to rewrite Dest");
	FallHookTestSupport::require(sourceIgnoredCatalog.records[0].data.stringID == 0x00035A2E, "sID-only identity was not preserved");
	FallHookTestSupport::require(sourceIgnoredCatalog.records[1].data.replacerText == "Blank source works", "blank Source catalog entry did not preserve Dest");
	FallHookTestSupport::require(sourceIgnoredCatalog.records[1].data.stringID == 0x00035A2F, "blank Source sID identity was not preserved");

	std::vector<std::uint8_t> plugin;
	std::vector<std::uint8_t> quest;
	FallHookTestSupport::appendVector(quest, FallHookTestSupport::subrecordString("EDID", "PlainQuest"));
	FallHookTestSupport::appendVector(quest, FallHookTestSupport::subrecord("QOBJ", FallHookTestSupport::bytesU16(42)));
	FallHookTestSupport::appendVector(quest, FallHookTestSupport::subrecord("NNAM", FallHookTestSupport::bytesU32(0x0000B001)));
	FallHookTestSupport::appendVector(plugin, FallHookTestSupport::record("QUST", 0x00009999, quest));
	const auto path = FallHookTestSupport::writeTempBinary("catalog_index.esp", plugin);
	const auto index = PluginEdidIndex::Load(path, { PluginEdidIndex::SignatureKey("QUST") });

	XmlTranslationEntry plainEdidEntry;
	plainEdidEntry.edid = "PlainQuest";
	plainEdidEntry.record = "QUST:NNAM";
	plainEdidEntry.dest = "Resolved by plugin form id";
	plainEdidEntry.index = 4;
	plainEdidEntry.stringID = 0x0000B001;

	XmlTranslationFile plainEdidFile;
	plainEdidFile.path = "plain.xml";
	plainEdidFile.addon = "Example.esp";
	plainEdidFile.entries.push_back(std::move(plainEdidEntry));

	const std::array resolvedFiles{ TranslationCatalogFile{ plainEdidFile, 0, &index } };
	const auto resolvedCatalog = TranslationCatalog::Build(resolvedFiles, XmlLoadOrder::Mode::kPlugin);
	FallHookTestSupport::require(resolvedCatalog.records.size() == 1, "plugin-indexed catalog entry missing");
	FallHookTestSupport::require(resolvedCatalog.records[0].data.formID == 0x00009999, "plugin form ID was not preserved in catalog data");
	FallHookTestSupport::require(resolvedCatalog.records[0].data.index == 42, "plugin string ID index was not preserved in catalog data");
	FallHookTestSupport::require(resolvedCatalog.records[0].data.editorID == "PlainQuest", "plugin EDID metadata was not preserved");
	FallHookTestSupport::require(resolvedCatalog.records[0].key == "p=*|f=00009999|e=|t=7|i=42|sid=45057", "plugin form/string ID was not used in source-free key");
	FallHookTestSupport::require(resolvedCatalog.pluginStringIDIndexes == 1, "catalog did not count plugin string ID index use");
	std::filesystem::remove(path);

	std::vector<std::uint8_t> questLogPlugin;
	std::vector<std::uint8_t> questLog;
	FallHookTestSupport::appendVector(questLog, FallHookTestSupport::subrecordString("EDID", "MQ102"));
	FallHookTestSupport::appendVector(questLog, FallHookTestSupport::subrecord("INDX", FallHookTestSupport::bytesU16(100)));
	FallHookTestSupport::appendVector(questLog, FallHookTestSupport::subrecord("QSDT", std::span<const std::uint8_t>{}));
	FallHookTestSupport::appendVector(questLog, FallHookTestSupport::subrecord("CNAM", FallHookTestSupport::bytesU32(0x0002DD9C)));
	FallHookTestSupport::appendVector(questLogPlugin, FallHookTestSupport::record("QUST", 0x0001ED86, questLog));
	const auto questLogPath = FallHookTestSupport::writeTempBinary("quest_log_index.esm", questLogPlugin);
	const auto questLogIndex = PluginEdidIndex::Load(questLogPath, { PluginEdidIndex::SignatureKey("QUST") });

	XmlTranslationEntry questLogEntry;
	questLogEntry.edid = "MQ102";
	questLogEntry.record = "QUST:CNAM";
	questLogEntry.dest = "Translated quest log";
	questLogEntry.index = 3;
	questLogEntry.stringID = 0x0002DD9C;

	XmlTranslationFile questLogFile;
	questLogFile.path = "quest_log.xml";
	questLogFile.addon = "Fallout4.esm";
	questLogFile.entries.push_back(std::move(questLogEntry));

	const std::array questLogFiles{ TranslationCatalogFile{ questLogFile, 0, &questLogIndex } };
	const auto questLogCatalog = TranslationCatalog::Build(questLogFiles, XmlLoadOrder::Mode::kPlugin);
	FallHookTestSupport::require(questLogCatalog.records.size() == 1, "QUST:CNAM catalog entry missing");
	FallHookTestSupport::require(questLogCatalog.records[0].data.translationType == TranslationType::kRuntimeLegacy, "QUST:CNAM type mismatch");
	FallHookTestSupport::require(questLogCatalog.records[0].data.formID == 0x0001ED86, "QUST:CNAM form ID mismatch");
	FallHookTestSupport::require(questLogCatalog.records[0].data.index == 100, "QUST:CNAM string ID index mismatch");
	FallHookTestSupport::require(questLogCatalog.records[0].data.stringID == 0x0002DD9C, "QUST:CNAM string ID was not preserved");
	std::filesystem::remove(questLogPath);

	std::vector<std::uint8_t> infoPlugin;
	std::vector<std::uint8_t> info;
	std::vector<std::uint8_t> trdt(13, 0);
	trdt[12] = 2;
	FallHookTestSupport::appendVector(info, FallHookTestSupport::subrecordString("EDID", "DialogueInfo"));
	FallHookTestSupport::appendVector(info, FallHookTestSupport::subrecord("TRDT", trdt));
	FallHookTestSupport::appendVector(info, FallHookTestSupport::subrecord("NAM1", FallHookTestSupport::bytesU32(0x0000A001)));
	FallHookTestSupport::appendVector(info, FallHookTestSupport::subrecord("RNAM", FallHookTestSupport::bytesU32(0x0000A002)));
	FallHookTestSupport::appendVector(infoPlugin, FallHookTestSupport::record("INFO", 0x00008888, info));
	const auto infoPath = FallHookTestSupport::writeTempBinary("dialogue_info_index.esm", infoPlugin);
	const auto infoIndex = PluginEdidIndex::Load(infoPath, { PluginEdidIndex::SignatureKey("INFO") });

	XmlTranslationEntry responseEntry;
	responseEntry.edid = "DialogueInfo";
	responseEntry.record = "INFO:NAM1";
	responseEntry.source = "";
	responseEntry.dest = "Translated response";
	responseEntry.index = 9;
	responseEntry.stringID = 0x0000A001;

	XmlTranslationEntry promptEntry;
	promptEntry.edid = "DialogueInfo";
	promptEntry.record = "INFO:RNAM";
	promptEntry.dest = "Translated prompt";
	promptEntry.index = 9;
	promptEntry.stringID = 0x0000A002;

	XmlTranslationEntry emptyResponseEntry;
	emptyResponseEntry.edid = "DialogueInfo";
	emptyResponseEntry.record = "INFO:NAM1";
	emptyResponseEntry.dest = " \t";
	emptyResponseEntry.stringID = 0x0000A003;

	XmlTranslationFile infoFile;
	infoFile.path = "dialogue_info.xml";
	infoFile.addon = "Fallout4.esm";
	infoFile.entries.push_back(std::move(responseEntry));
	infoFile.entries.push_back(std::move(promptEntry));
	infoFile.entries.push_back(std::move(emptyResponseEntry));

	const std::array infoFiles{ TranslationCatalogFile{ infoFile, 0, &infoIndex } };
	const auto infoCatalog = TranslationCatalog::Build(infoFiles, XmlLoadOrder::Mode::kPlugin);
	FallHookTestSupport::require(infoCatalog.records.size() == 2, "INFO dialogue catalog entries missing");
	FallHookTestSupport::require(infoCatalog.skippedEmptyDest == 1, "empty INFO:NAM1 Dest should be skipped");
	FallHookTestSupport::require(infoCatalog.records[0].data.translationType == TranslationType::kRuntimeIndex, "INFO:NAM1 type mismatch");
	FallHookTestSupport::require(infoCatalog.records[0].data.formID == 0x00008888, "INFO:NAM1 form ID mismatch");
	FallHookTestSupport::require(infoCatalog.records[0].data.index == 0, "INFO:NAM1 string ID index mismatch");
	FallHookTestSupport::require(infoCatalog.records[0].data.stringID == 0x0000A001, "INFO:NAM1 string ID was not preserved");
	FallHookTestSupport::require(infoCatalog.records[1].data.translationType == TranslationType::kRuntime2, "INFO:RNAM type mismatch");
	FallHookTestSupport::require(infoCatalog.records[1].data.index == 0, "INFO:RNAM prompt index mismatch");
	FallHookTestSupport::require(infoCatalog.records[1].data.stringID == 0x0000A002, "INFO:RNAM string ID was not preserved");
	std::filesystem::remove(infoPath);

	std::vector<std::uint8_t> furniturePlugin;
	std::vector<std::uint8_t> furniture;
	FallHookTestSupport::appendVector(furniture, FallHookTestSupport::subrecordString("EDID", "PowerArmorFurniture"));
	FallHookTestSupport::appendVector(furniture, FallHookTestSupport::subrecord("ATTX", FallHookTestSupport::bytesU32(0x00012AC9)));
	FallHookTestSupport::appendVector(furniturePlugin, FallHookTestSupport::record("FURN", 0x0000F123, furniture));
	const auto furniturePath = FallHookTestSupport::writeTempBinary("furn_attx_index.esm", furniturePlugin);
	const auto furnitureIndexData = PluginEdidIndex::Load(furniturePath, { PluginEdidIndex::SignatureKey("FURN") });

	XmlTranslationEntry furnitureEntry;
	furnitureEntry.edid = "PowerArmorFurniture";
	furnitureEntry.record = "FURN:ATTX";
	furnitureEntry.dest = "Vao";
	furnitureEntry.stringID = 0x00012AC9;

	XmlTranslationFile furnitureFile;
	furnitureFile.path = "furn_attx.xml";
	furnitureFile.addon = "Fallout4.esm";
	furnitureFile.entries.push_back(std::move(furnitureEntry));

	const std::array furnitureFiles{ TranslationCatalogFile{ furnitureFile, 0, &furnitureIndexData } };
	const auto furnitureCatalog = TranslationCatalog::Build(furnitureFiles, XmlLoadOrder::Mode::kPlugin);
	FallHookTestSupport::require(furnitureCatalog.records.size() == 1, "FURN:ATTX catalog entry missing");
	FallHookTestSupport::require(furnitureCatalog.records[0].recordSignature == "FURN ATTX", "FURN:ATTX signature mismatch");
	FallHookTestSupport::require(furnitureCatalog.records[0].data.translationType == TranslationType::kActivationText, "FURN:ATTX type mismatch");
	FallHookTestSupport::require(furnitureCatalog.records[0].data.formID == 0x0000F123, "FURN:ATTX form ID mismatch");
	FallHookTestSupport::require(!furnitureCatalog.records[0].data.index, "FURN:ATTX should not have a plugin string index");
	FallHookTestSupport::require(furnitureCatalog.records[0].data.stringID == 0x00012AC9, "FURN:ATTX string ID was not preserved");
	FallHookTestSupport::require(furnitureCatalog.records[0].data.editorID && *furnitureCatalog.records[0].data.editorID == "PowerArmorFurniture", "FURN:ATTX EDID metadata was not preserved");
	FallHookTestSupport::require(furnitureCatalog.records[0].data.replacerText == "Vao", "FURN:ATTX destination mismatch");
	FallHookTestSupport::require(furnitureCatalog.pluginStringIDIndexes == 0, "FURN:ATTX should not count plugin string ID index use");
	const auto preparedFurniture = TranslationPreparedDataBuilder::Build(furnitureCatalog);
	FallHookTestSupport::require(preparedFurniture.activationText.records.size() == 1, "FURN:ATTX was not prepared as activation text");
	FallHookTestSupport::require(preparedFurniture.constApply.records.empty(), "FURN:ATTX should not enter direct const apply");
	std::filesystem::remove(furniturePath);

	std::vector<std::uint8_t> perkPlugin;
	std::vector<std::uint8_t> perk;
	const std::array<std::uint8_t, 1> activateChoiceType{ 4 };
	FallHookTestSupport::appendVector(perk, FallHookTestSupport::subrecordString("EDID", "RoboticsExpert01"));
	FallHookTestSupport::appendVector(perk, FallHookTestSupport::subrecord("EPFT", std::span<const std::uint8_t>{ activateChoiceType.data(), activateChoiceType.size() }));
	FallHookTestSupport::appendVector(perk, FallHookTestSupport::subrecord("EPF2", FallHookTestSupport::bytesU32(0x0000E001)));
	std::vector<std::uint8_t> perkIndex;
	FallHookTestSupport::appendU16(perkIndex, 3);
	FallHookTestSupport::appendU16(perkIndex, 0);
	FallHookTestSupport::appendVector(perk, FallHookTestSupport::subrecord("EPF3", perkIndex));
	FallHookTestSupport::appendVector(perkPlugin, FallHookTestSupport::record("PERK", 0x00006789, perk));
	const auto perkPath = FallHookTestSupport::writeTempBinary("perk_epf2_index.esm", perkPlugin);
	const auto perkIndexData = PluginEdidIndex::Load(perkPath, { PluginEdidIndex::SignatureKey("PERK") });

	XmlTranslationEntry perkEntry;
	perkEntry.edid = "RoboticsExpert01";
	perkEntry.record = "PERK:EPF2";
	perkEntry.dest = "HACK";
	perkEntry.index = 9;
	perkEntry.stringID = 0x0000E001;

	XmlTranslationFile perkFile;
	perkFile.path = "perk_epf2.xml";
	perkFile.addon = "Fallout4.esm";
	perkFile.entries.push_back(std::move(perkEntry));

	const std::array perkFiles{ TranslationCatalogFile{ perkFile, 0, &perkIndexData } };
	const auto perkCatalog = TranslationCatalog::Build(perkFiles, XmlLoadOrder::Mode::kPlugin);
	FallHookTestSupport::require(perkCatalog.records.size() == 1, "PERK:EPF2 catalog entry missing");
	FallHookTestSupport::require(perkCatalog.records[0].recordSignature == "PERK EPF2", "PERK:EPF2 signature mismatch");
	FallHookTestSupport::require(perkCatalog.records[0].data.translationType == TranslationType::kButtonText2, "PERK:EPF2 type mismatch");
	FallHookTestSupport::require(perkCatalog.records[0].data.formID == 0x00006789, "PERK:EPF2 form ID mismatch");
	FallHookTestSupport::require(perkCatalog.records[0].data.index == 3, "PERK:EPF2 string ID index mismatch");
	FallHookTestSupport::require(perkCatalog.records[0].data.stringID == 0x0000E001, "PERK:EPF2 string ID was not preserved");
	FallHookTestSupport::require(perkCatalog.records[0].data.editorID && *perkCatalog.records[0].data.editorID == "RoboticsExpert01", "PERK:EPF2 EDID metadata was not preserved");
	FallHookTestSupport::require(perkCatalog.records[0].data.replacerText == "HACK", "PERK:EPF2 destination mismatch");
	FallHookTestSupport::require(perkCatalog.pluginStringIDIndexes == 1, "PERK:EPF2 did not count plugin string ID index use");
	const auto preparedPerk = TranslationPreparedDataBuilder::Build(perkCatalog);
	FallHookTestSupport::require(preparedPerk.perkActivateChoice.records.size() == 1, "PERK:EPF2 was not prepared as perk activate choice");
	FallHookTestSupport::require(preparedPerk.activationText.records.empty(), "PERK:EPF2 should not enter activation text");
	std::filesystem::remove(perkPath);

	std::vector<std::uint8_t> bodyPartPlugin;
	std::vector<std::uint8_t> bodyPart;
	FallHookTestSupport::appendVector(bodyPart, FallHookTestSupport::subrecordString("EDID", "CaveCricketBodyPartData"));
	FallHookTestSupport::appendVector(bodyPart, FallHookTestSupport::subrecord("BPTN", FallHookTestSupport::bytesU32(0x0001D850)));
	FallHookTestSupport::appendVector(bodyPart, FallHookTestSupport::subrecordString("BPNN", "CCRKT_BN_R_Ankle_F"));
	FallHookTestSupport::appendVector(bodyPart, FallHookTestSupport::subrecordString("BPNT", "CCRKT_BN_R_Ankle_F"));
	FallHookTestSupport::appendVector(bodyPart, FallHookTestSupport::subrecord("BPND", bodyPartData(24)));
	FallHookTestSupport::appendVector(bodyPartPlugin, FallHookTestSupport::record("BPTD", 0x0000AB01, bodyPart));
	const auto bodyPartPath = FallHookTestSupport::writeTempBinary("bptd_index.esm", bodyPartPlugin);
	const auto bodyPartIndex = PluginEdidIndex::Load(bodyPartPath, { PluginEdidIndex::SignatureKey("BPTD") });

	XmlTranslationEntry bodyPartEntry;
	bodyPartEntry.edid = "CaveCricketBodyPartData";
	bodyPartEntry.record = "BPTD:BPTN";
	bodyPartEntry.dest = "Chan phai";
	bodyPartEntry.index = 0;
	bodyPartEntry.stringID = 0x0001D850;

	XmlTranslationFile bodyPartFile;
	bodyPartFile.path = "bptd.xml";
	bodyPartFile.addon = "Fallout4.esm";
	bodyPartFile.entries.push_back(std::move(bodyPartEntry));

	const std::array bodyPartFiles{ TranslationCatalogFile{ bodyPartFile, 0, &bodyPartIndex } };
	const auto bodyPartCatalog = TranslationCatalog::Build(bodyPartFiles, XmlLoadOrder::Mode::kPlugin);
	FallHookTestSupport::require(bodyPartCatalog.records.size() == 1, "BPTD:BPTN catalog entry missing");
	FallHookTestSupport::require(bodyPartCatalog.records[0].data.translationType == TranslationType::kBodyPartName, "BPTD:BPTN type mismatch");
	FallHookTestSupport::require(bodyPartCatalog.records[0].data.formID == 0x0000AB01, "BPTD:BPTN form ID mismatch");
	FallHookTestSupport::require(bodyPartCatalog.records[0].data.index == 24, "BPTD:BPTN plugin slot mismatch");
	FallHookTestSupport::require(bodyPartCatalog.records[0].data.stringID == 0x0001D850, "BPTD:BPTN string ID was not preserved");
	FallHookTestSupport::require(bodyPartCatalog.pluginStringIDIndexes == 1, "BPTD:BPTN did not count plugin string ID index use");
	std::filesystem::remove(bodyPartPath);

	std::vector<std::uint8_t> weaponPlugin;
	std::vector<std::uint8_t> weapon;
	FallHookTestSupport::appendVector(weapon, FallHookTestSupport::subrecordString("EDID", "PipeRevolver"));
	FallHookTestSupport::appendVector(weapon, FallHookTestSupport::subrecord("FULL", FallHookTestSupport::bytesU32(0x0000AA00)));
	FallHookTestSupport::appendVector(weapon, FallHookTestSupport::subrecord("FULL", FallHookTestSupport::bytesU32(0x0000AA01)));
	FallHookTestSupport::appendVector(weapon, FallHookTestSupport::subrecord("FULL", FallHookTestSupport::bytesU32(0x0000AA02)));
	FallHookTestSupport::appendVector(weaponPlugin, FallHookTestSupport::record("WEAP", 0x0000AC01, weapon));
	const auto weaponPath = FallHookTestSupport::writeTempBinary("weap_full_index.esm", weaponPlugin);
	const auto weaponIndex = PluginEdidIndex::Load(weaponPath, { PluginEdidIndex::SignatureKey("WEAP") });

	XmlTranslationEntry weaponEntry;
	weaponEntry.edid = "PipeRevolver";
	weaponEntry.record = "WEAP:FULL";
	weaponEntry.dest = "Sung luc";
	weaponEntry.index = 5;
	weaponEntry.stringID = 0x0000AA01;

	XmlTranslationFile weaponFile;
	weaponFile.path = "weap_full.xml";
	weaponFile.addon = "Fallout4.esm";
	weaponFile.entries.push_back(std::move(weaponEntry));

	const std::array weaponFiles{ TranslationCatalogFile{ weaponFile, 0, &weaponIndex } };
	const auto weaponCatalog = TranslationCatalog::Build(weaponFiles, XmlLoadOrder::Mode::kPlugin);
	FallHookTestSupport::require(weaponCatalog.records.size() == 1, "WEAP:FULL catalog entry missing");
	FallHookTestSupport::require(weaponCatalog.records[0].data.translationType == TranslationType::kFullName, "WEAP:FULL type mismatch");
	FallHookTestSupport::require(weaponCatalog.records[0].data.formID == 0x0000AC01, "WEAP:FULL form ID mismatch");
	FallHookTestSupport::require(
		weaponCatalog.records[0].data.index == PluginEdidIndex::EncodeTemplateFullNameSlot(0),
		"WEAP:FULL plugin template slot mismatch");
	FallHookTestSupport::require(weaponCatalog.records[0].data.stringID == 0x0000AA01, "WEAP:FULL string ID was not preserved");
	FallHookTestSupport::require(weaponCatalog.pluginStringIDIndexes == 1, "WEAP:FULL did not count plugin string ID index use");
	const auto preparedWeapon = TranslationPreparedDataBuilder::Build(weaponCatalog);
	FallHookTestSupport::require(preparedWeapon.inventoryTemplateNames.records.size() == 1, "WEAP:FULL template slot was not prepared");
	FallHookTestSupport::require(preparedWeapon.fullNameLoad.records.empty(), "WEAP:FULL template slot should not enter full-name-load");
	FallHookTestSupport::require(preparedWeapon.constApply.records.empty(), "WEAP:FULL template slot should not enter const apply");
	std::filesystem::remove(weaponPath);
}

void testTranslationPipeline()
{
	const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
	const auto root = std::filesystem::temp_directory_path() / std::format("FallHook_pipeline_{}", stamp);
	const auto dataDir = root / "Data";
	const auto xmlDir = dataDir / "F4SE" / "Plugins" / "FallHook";
	std::filesystem::create_directories(xmlDir);

	std::vector<std::uint8_t> quest;
	FallHookTestSupport::appendVector(quest, FallHookTestSupport::subrecordString("EDID", "PipelineQuest"));
	FallHookTestSupport::appendVector(quest, FallHookTestSupport::subrecord("QOBJ", FallHookTestSupport::bytesU16(7)));
	FallHookTestSupport::appendVector(quest, FallHookTestSupport::subrecord("NNAM", FallHookTestSupport::bytesU32(0x0000A111)));
	const auto pluginBytes = FallHookTestSupport::record("QUST", 0x00001234, quest);
	FallHookTestSupport::writeBinaryFile(dataDir / "Example.esp", pluginBytes);

	const auto xmlPath = xmlDir / "Example.xml";
	std::ofstream xml(xmlPath, std::ios::binary);
	xml << R"(<SSTXMLRessources><Params><Addon>Example.esp</Addon></Params><Content>
<String sID="0000A111"><EDID>PipelineQuest</EDID><REC id="99">QUST:NNAM</REC><Source>Old</Source><Dest>New</Dest></String>
</Content></SSTXMLRessources>)";
	xml.close();

	TranslationPipelineOptions options;
	options.xmlDirectory = xmlDir;
	options.dataDirectory = dataDir;
	options.plugins.push_back({ "Example.esp", dataDir / "Example.esp", 3 });
	const auto result = TranslationPipeline::Build(options);

	FallHookTestSupport::require(result.discoveredXmlFiles == 1, "pipeline XML discovery mismatch");
	FallHookTestSupport::require(!result.loadedFromCache, "first pipeline build should not load cache");
	FallHookTestSupport::require(result.savedCache, "first pipeline build should save cache");
	FallHookTestSupport::require(result.parsedXmlFiles == 1, "pipeline XML parse count mismatch");
	FallHookTestSupport::require(result.loadedPluginIndexes == 1, "pipeline plugin index load mismatch");
	FallHookTestSupport::require(result.catalog.records.size() == 1, "pipeline catalog record count mismatch");
	FallHookTestSupport::require(result.catalog.records[0].data.formID == 0x00001234, "pipeline catalog form ID mismatch");
	FallHookTestSupport::require(result.catalog.records[0].data.index == 7, "pipeline catalog SID index mismatch");
	FallHookTestSupport::require(result.catalog.records[0].data.replacerText == "New", "pipeline catalog destination mismatch");
	FallHookTestSupport::require(result.prepared.constApply.records.size() == 1, "pipeline prepared const-apply section missing");
	FallHookTestSupport::require(result.prepared.questJournal.records.empty(), "pipeline prepared quest journal section should be empty");

	const auto cached = TranslationPipeline::Build(options);
	FallHookTestSupport::require(cached.loadedFromCache, "second pipeline build should load cache");
	FallHookTestSupport::require(!cached.savedCache, "cached pipeline build should not rewrite cache");
	FallHookTestSupport::require(cached.cachePath.filename() == "FallHook.runtime.cache", "pipeline cache path mismatch");
	FallHookTestSupport::require(cached.catalog.records.size() == 1, "cached pipeline record count mismatch");
	FallHookTestSupport::require(cached.catalog.records[0].data.formID == 0x00001234, "cached pipeline form ID mismatch");
	FallHookTestSupport::require(cached.catalog.records[0].data.replacerText == "New", "cached pipeline destination mismatch");
	FallHookTestSupport::require(cached.prepared.constApply.records.size() == 1, "cached prepared const-apply section missing");
	FallHookTestSupport::require(cached.prepared.constApply.records[0].data.replacerText == "New", "cached prepared destination mismatch");


	std::filesystem::remove_all(root);
}
