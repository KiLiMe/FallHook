// AI CONTEXT: Tests plugin EDID and string-ID indexing from Fallout 4 binary records.
// Depends on PluginEdidIndex and shared binary fixture helpers.
// Runtime scope is Fallout 4 1.10.163 plugin data-file parsing without loading the game.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: verifies lookup by plugin/form/string identity, not original text.
#include "FallHookTestSupport.h"
#include "InstanceNamingRuleIdentity.h"
#include "PluginEdidIndex.h"

#include <array>
#include <filesystem>
#include <span>
#include <unordered_set>
#include <vector>

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
}

void testPluginEdidIndex()
{
	std::vector<std::uint8_t> plugin;

	std::vector<std::uint8_t> uniqueNpc;
	FallHookTestSupport::appendVector(uniqueNpc, FallHookTestSupport::subrecordString("EDID", "UniqueNPC"));
	FallHookTestSupport::appendVector(plugin, FallHookTestSupport::record("NPC_", 0x00001001, uniqueNpc));

	std::vector<std::uint8_t> duplicateNpc;
	FallHookTestSupport::appendVector(duplicateNpc, FallHookTestSupport::subrecordString("EDID", "DuplicateNPC"));
	FallHookTestSupport::appendVector(plugin, FallHookTestSupport::record("NPC_", 0x00001002, duplicateNpc));
	FallHookTestSupport::appendVector(plugin, FallHookTestSupport::record("NPC_", 0x00001003, duplicateNpc));

	std::vector<std::uint8_t> info;
	auto makeTrda = [](std::uint32_t responseID) {
		std::vector<std::uint8_t> payload;
		FallHookTestSupport::appendU32(payload, 0); // offset 0 (unused prefix)
		FallHookTestSupport::appendU32(payload, responseID); // offset 4 = response ID
		return payload;
	};
	FallHookTestSupport::appendVector(info, FallHookTestSupport::subrecord("TRDA", makeTrda(7)));
	FallHookTestSupport::appendVector(info, FallHookTestSupport::subrecord("NAM1", FallHookTestSupport::bytesU32(0x0000A001)));
	FallHookTestSupport::appendVector(info, FallHookTestSupport::subrecord("TRDA", makeTrda(9)));
	FallHookTestSupport::appendVector(info, FallHookTestSupport::subrecord("NAM1", FallHookTestSupport::bytesU32(0x0000A003)));
	FallHookTestSupport::appendVector(info, FallHookTestSupport::subrecord("RNAM", FallHookTestSupport::bytesU32(0x0000A002)));
	FallHookTestSupport::appendVector(plugin, FallHookTestSupport::record("INFO", 0x00001234, info));

	std::vector<std::uint8_t> childInfo;
	FallHookTestSupport::appendVector(childInfo, FallHookTestSupport::subrecord("GNAM", FallHookTestSupport::bytesU32(0x00001234)));
	FallHookTestSupport::appendVector(plugin, FallHookTestSupport::record("INFO", 0x00001235, childInfo));

	std::vector<std::uint8_t> quest;
	FallHookTestSupport::appendVector(quest, FallHookTestSupport::subrecord("QOBJ", FallHookTestSupport::bytesU16(42)));
	FallHookTestSupport::appendVector(quest, FallHookTestSupport::subrecord("NNAM", FallHookTestSupport::bytesU32(0x0000B001)));
	FallHookTestSupport::appendVector(quest, FallHookTestSupport::subrecord("INDX", FallHookTestSupport::bytesU16(100)));
	FallHookTestSupport::appendVector(quest, FallHookTestSupport::subrecord("QSDT", std::span<const std::uint8_t>{}));
	FallHookTestSupport::appendVector(quest, FallHookTestSupport::subrecord("CNAM", FallHookTestSupport::bytesU32(0x0000B002)));
	FallHookTestSupport::appendVector(quest, FallHookTestSupport::subrecord("QSDT", std::span<const std::uint8_t>{}));
	FallHookTestSupport::appendVector(quest, FallHookTestSupport::subrecord("CNAM", FallHookTestSupport::bytesU32(0x0000B003)));
	FallHookTestSupport::appendVector(plugin, FallHookTestSupport::record("QUST", 0x00003456, quest));

	std::vector<std::uint8_t> message;
	FallHookTestSupport::appendVector(message, FallHookTestSupport::subrecord("ITXT", FallHookTestSupport::bytesU32(0x0000D001)));
	FallHookTestSupport::appendVector(message, FallHookTestSupport::subrecord("ITXT", FallHookTestSupport::bytesU32(0x0000D002)));
	FallHookTestSupport::appendVector(plugin, FallHookTestSupport::record("MESG", 0x00005678, message));

	std::vector<std::uint8_t> perk;
	const std::array<std::uint8_t, 1> activateChoiceType{ 4 };
	FallHookTestSupport::appendVector(perk, FallHookTestSupport::subrecord("EPFT", std::span<const std::uint8_t>{ activateChoiceType.data(), activateChoiceType.size() }));
	FallHookTestSupport::appendVector(perk, FallHookTestSupport::subrecord("EPF2", FallHookTestSupport::bytesU32(0x0000E001)));
	std::vector<std::uint8_t> perkIndex;
	FallHookTestSupport::appendU16(perkIndex, 3);
	FallHookTestSupport::appendU16(perkIndex, 0);
	FallHookTestSupport::appendVector(perk, FallHookTestSupport::subrecord("EPF3", perkIndex));
	const std::array<std::uint8_t, 1> textType{ 7 };
	FallHookTestSupport::appendVector(perk, FallHookTestSupport::subrecord("EPFT", std::span<const std::uint8_t>{ textType.data(), textType.size() }));
	FallHookTestSupport::appendVector(perk, FallHookTestSupport::subrecord("EPFD", FallHookTestSupport::bytesU32(0x0000E002)));
	FallHookTestSupport::appendVector(plugin, FallHookTestSupport::record("PERK", 0x00006789, perk));

	std::vector<std::uint8_t> terminal;
	FallHookTestSupport::appendVector(terminal, FallHookTestSupport::subrecordString("EDID", "TerminalWithStrings"));
	FallHookTestSupport::appendVector(terminal, FallHookTestSupport::subrecord("BTXT", FallHookTestSupport::bytesU32(0x0000C001)));
	FallHookTestSupport::appendVector(terminal, FallHookTestSupport::subrecord("ITXT", FallHookTestSupport::bytesU32(0x0000C002)));
	FallHookTestSupport::appendVector(terminal, FallHookTestSupport::subrecord("RNAM", FallHookTestSupport::bytesU32(0x0000C003)));
	FallHookTestSupport::appendVector(terminal, FallHookTestSupport::subrecord("UNAM", FallHookTestSupport::bytesU32(0x0000C004)));
	FallHookTestSupport::appendVector(plugin, FallHookTestSupport::record("TERM", 0x00004567, FallHookTestSupport::compressedPayload(terminal), 0x00040000));

	std::vector<std::uint8_t> instanceNaming;
	FallHookTestSupport::appendVector(instanceNaming, FallHookTestSupport::subrecordString("EDID", "dn_Test"));
	FallHookTestSupport::appendVector(instanceNaming, FallHookTestSupport::subrecord("VNAM", FallHookTestSupport::bytesU32(2)));
	FallHookTestSupport::appendVector(instanceNaming, FallHookTestSupport::subrecord("WNAM", FallHookTestSupport::bytesU32(0x0000F001)));
	FallHookTestSupport::appendVector(instanceNaming, FallHookTestSupport::subrecord("WNAM", FallHookTestSupport::bytesU32(0)));
	FallHookTestSupport::appendVector(instanceNaming, FallHookTestSupport::subrecord("VNAM", FallHookTestSupport::bytesU32(1)));
	FallHookTestSupport::appendVector(instanceNaming, FallHookTestSupport::subrecord("WNAM", FallHookTestSupport::bytesU32(0x0000F002)));
	FallHookTestSupport::appendVector(plugin, FallHookTestSupport::record("INNR", 0x0000789A, instanceNaming));

	std::vector<std::uint8_t> bodyPart;
	FallHookTestSupport::appendVector(bodyPart, FallHookTestSupport::subrecordString("EDID", "TestBodyPartData"));
	FallHookTestSupport::appendVector(bodyPart, FallHookTestSupport::subrecord("BPTN", FallHookTestSupport::bytesU32(0x0001D850)));
	FallHookTestSupport::appendVector(bodyPart, FallHookTestSupport::subrecordString("BPNN", "RightFootNode"));
	FallHookTestSupport::appendVector(bodyPart, FallHookTestSupport::subrecordString("BPNT", "RightFootTarget"));
	FallHookTestSupport::appendVector(bodyPart, FallHookTestSupport::subrecord("BPND", bodyPartData(24)));
	FallHookTestSupport::appendVector(plugin, FallHookTestSupport::record("BPTD", 0x0000AB01, bodyPart));

	std::vector<std::uint8_t> weapon;
	FallHookTestSupport::appendVector(weapon, FallHookTestSupport::subrecordString("EDID", "PipeRevolver"));
	FallHookTestSupport::appendVector(weapon, FallHookTestSupport::subrecord("FULL", FallHookTestSupport::bytesU32(0x0000AA00)));
	FallHookTestSupport::appendVector(weapon, FallHookTestSupport::subrecord("FULL", FallHookTestSupport::bytesU32(0x0000AA01)));
	FallHookTestSupport::appendVector(weapon, FallHookTestSupport::subrecord("FULL", FallHookTestSupport::bytesU32(0x0000AA02)));
	FallHookTestSupport::appendVector(plugin, FallHookTestSupport::record("WEAP", 0x0000AC01, weapon));

	const auto path = FallHookTestSupport::writeTempBinary("plugin_index.esp", plugin);
	const auto index = PluginEdidIndex::Load(
		path,
		std::unordered_set<std::uint32_t>{
			PluginEdidIndex::SignatureKey("NPC_"),
			PluginEdidIndex::SignatureKey("INFO"),
			PluginEdidIndex::SignatureKey("QUST"),
			PluginEdidIndex::SignatureKey("MESG"),
			PluginEdidIndex::SignatureKey("PERK"),
			PluginEdidIndex::SignatureKey("TERM"),
			PluginEdidIndex::SignatureKey("INNR"),
			PluginEdidIndex::SignatureKey("BPTD"),
			PluginEdidIndex::SignatureKey("WEAP")
		});

	FallHookTestSupport::require(index.loaded(), "plugin EDID index should mark readable files loaded");
	FallHookTestSupport::require(index.recordsVisited() == 12, "plugin EDID index visited record count mismatch");
	FallHookTestSupport::require(index.recordsScanned() == 12, "plugin EDID index scanned record count mismatch");
	FallHookTestSupport::require(index.lookup("npc_", "uniquenpc") == 0x00001001, "case-insensitive EDID lookup mismatch");
	FallHookTestSupport::require(!index.lookup("NPC_", "DuplicateNPC").has_value(), "duplicate EDID should be ambiguous");
	FallHookTestSupport::require(index.lookup("TERM", "TerminalWithStrings") == 0x00004567, "compressed terminal EDID lookup mismatch");
	FallHookTestSupport::require(index.compressedRecords() == 1 && index.failedCompressedRecords() == 0, "compressed record counter mismatch");
	FallHookTestSupport::require(index.lookupInfoResponseIndex(0x00001234, 0x0000A003) == 1, "INFO:NAM1 string ID index mismatch");
	FallHookTestSupport::require(index.lookupInfoResponseIDIndex(0x00001234, 9) == 1, "INFO response ID index mismatch");
	FallHookTestSupport::require(index.lookupInfoPromptIndex(0x00001234, 0x0000A002) == 0, "INFO:RNAM string ID index mismatch");
	FallHookTestSupport::require(index.lookupInfoParentGroup(0x00000235) == 0x00001234, "INFO parent compact lookup mismatch");
	FallHookTestSupport::require(index.lookupQuestObjectiveIndex(0x00003456, 0x0000B001) == 42, "QUST:NNAM string ID index mismatch");
	FallHookTestSupport::require(index.lookupQuestStageLogIndex(0x00000456, 0x0000B003) == 101, "QUST:CNAM compact lookup mismatch");
	FallHookTestSupport::require(index.lookupMessageButtonIndex(0x00005678, 0x0000D002) == 1, "MESG:ITXT string ID index mismatch");
	FallHookTestSupport::require(index.lookupPerkActivateChoiceIndex(0x00006789, 0x0000E001) == 3, "PERK activate choice index mismatch");
	FallHookTestSupport::require(index.lookupPerkTextIndex(0x00006789, 0x0000E002) == 0, "PERK text index mismatch");
	FallHookTestSupport::require(index.lookupTerminalBodyIndex(0x00004567, 0x0000C001) == 0, "TERM:BTXT index mismatch");
	FallHookTestSupport::require(index.lookupTerminalItemIndex(0x00004567, 0x0000C002) == 0, "TERM:ITXT index mismatch");
	FallHookTestSupport::require(index.lookupTerminalResponseIndex(0x00004567, 0x0000C003) == 0, "TERM:RNAM index mismatch");
	FallHookTestSupport::require(index.lookupTerminalResultIndex(0x00004567, 0x0000C004) == 0, "TERM:UNAM index mismatch");
	const auto emptyInnrHash = InstanceNamingRuleIdentity::HashSortedKeywords(std::span<const std::uint32_t>{}, 0);
	FallHookTestSupport::require(
		index.lookupInstanceNamingRuleSlot(0x0000789A, 0x0000F001) == PluginEdidIndex::EncodeInstanceNamingRuleConditionKey(0, emptyInnrHash),
		"INNR:WNAM first rule condition key mismatch");
	FallHookTestSupport::require(
		index.lookupInstanceNamingRuleSlot(0x0000089A, 0x0000F002) == PluginEdidIndex::EncodeInstanceNamingRuleConditionKey(1, emptyInnrHash),
		"INNR:WNAM compact rule condition key mismatch");
	FallHookTestSupport::require(index.lookupBodyPartNameSlot(0x00000B01, 0x0001D850) == 24, "BPTD:BPTN body-part slot mismatch");
	FallHookTestSupport::require(!index.lookupTemplateFullNameSlot(0x0000AC01, 0x0000AA00).has_value(), "base WEAP:FULL should not be a template slot");
	FallHookTestSupport::require(
		index.lookupTemplateFullNameSlot(0x0000AC01, 0x0000AA01) == PluginEdidIndex::EncodeTemplateFullNameSlot(0),
		"first template WEAP:FULL slot mismatch");
	FallHookTestSupport::require(
		index.lookupTemplateFullNameSlot(0x00000C01, 0x0000AA02) == PluginEdidIndex::EncodeTemplateFullNameSlot(1),
		"compact template WEAP:FULL slot mismatch");

	std::filesystem::remove(path);
}
