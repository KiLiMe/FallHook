// AI CONTEXT: Tests XML and TXT translation input parsers.
// Depends on parser modules and shared temporary-file test helpers.
// Runtime scope is Fallout 4 1.10.163 translation input semantics without loading the game.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: parser may read Source XML/TXT fields, but tests do not allow source-key lookup behavior.
#include "FallHookTestSupport.h"
#include "TxtTranslationParser.h"
#include "XmlTranslationParser.h"

#include <filesystem>

void testXmlParser()
{
	const auto path = FallHookTestSupport::writeTempFile("valid.xml", R"(<?xml version="1.0" encoding="UTF-8"?>
<SSTXMLRessources>
  <Params><Addon>Example.esp</Addon></Params>
  <Content>
    <String List="0" sID="000ABC" Partial="1">
      <EDID>ExampleQuest</EDID>
      <REC id="3" idMax="8">QUST:NNAM</REC>
      <Source>Old objective</Source>
      <Dest>New objective</Dest>
    </String>
    <String><EDID>ExampleQuest</EDID><REC>INFO:NAM1</REC><Dest>Translated response</Dest></String>
    <String sID="012AC9"><EDID>PowerArmorFurniture</EDID><REC>FURN:ATTX</REC><Source>Enter</Source><Dest>Vao</Dest></String>
    <String sID="00E001"><EDID>RoboticsExpert01</EDID><REC id="3">PERK:EPF2</REC><Source>Hack</Source><Dest>Hack</Dest></String>
  </Content>
</SSTXMLRessources>)");

	const auto parsed = XmlTranslationParser::ParseFile(path);
	FallHookTestSupport::require(parsed.success, parsed.error);
	FallHookTestSupport::require(parsed.file.addon == "Example.esp", "addon not parsed");
	FallHookTestSupport::require(parsed.file.entries.size() == 4, "entry count mismatch");
	FallHookTestSupport::require(parsed.file.entries[0].stringID == 0xABC, "sID mismatch");
	FallHookTestSupport::require(parsed.file.entries[0].index == 3, "REC id mismatch");
	FallHookTestSupport::require(parsed.file.entries[0].dest == "New objective", "Dest mismatch");
	FallHookTestSupport::require(parsed.file.entries[1].source.empty(), "missing Source should parse as empty");
	FallHookTestSupport::require(parsed.file.entries[2].edid == "PowerArmorFurniture", "FURN EDID mismatch");
	FallHookTestSupport::require(parsed.file.entries[2].record == "FURN:ATTX", "FURN REC mismatch");
	FallHookTestSupport::require(parsed.file.entries[2].stringID == 0x12AC9, "FURN sID mismatch");
	FallHookTestSupport::require(!parsed.file.entries[2].index, "FURN REC id should be absent");
	FallHookTestSupport::require(parsed.file.entries[3].edid == "RoboticsExpert01", "PERK EDID mismatch");
	FallHookTestSupport::require(parsed.file.entries[3].record == "PERK:EPF2", "PERK REC mismatch");
	FallHookTestSupport::require(parsed.file.entries[3].index == 3, "PERK REC id mismatch");
	FallHookTestSupport::require(parsed.file.entries[3].stringID == 0xE001, "PERK sID mismatch");
	std::filesystem::remove(path);
}

void testTxtParser()
{
	const auto path = FallHookTestSupport::writeTempFile("valid.txt", "\xEF\xBB\xBF# comment\n$NEW\tTranslated New\nTrailingTab\t\n");
	const auto parsed = TxtTranslationParser::ParseFile(path);
	FallHookTestSupport::require(parsed.success, parsed.error);
	FallHookTestSupport::require(parsed.file.entries.size() == 2, "TXT entry count mismatch");
	FallHookTestSupport::require(parsed.file.entries[0].source == "$NEW", "TXT source mismatch");
	FallHookTestSupport::require(parsed.file.entries[1].dest.empty(), "TXT empty destination should be preserved");
	std::filesystem::remove(path);
}
