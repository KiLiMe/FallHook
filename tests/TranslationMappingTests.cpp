// AI CONTEXT: Tests source-free translation type mapping and identity key helpers.
// Depends on runtime-resolution, load-order, mapping, and key helper modules.
// Runtime scope is Fallout 4 1.10.163 data semantics without loading the game.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: verifies keys are built from stable identity fields instead of source text.
#include "FallHookTestSupport.h"
#include "RuntimeResolution.h"
#include "SourceFreeTranslationKey.h"
#include "XmlLoadOrder.h"
#include "XmlTranslationMapping.h"

void testMapping()
{
	FallHookTestSupport::require(XmlTranslationMapping::NormalizeSignature(" qust : nnam ") == "QUST NNAM", "signature normalization failed");
	FallHookTestSupport::require(XmlTranslationMapping::ParseBracketFormID("[01001234]") == 0x01001234, "bracket FormID parse failed");
	FallHookTestSupport::require(!XmlTranslationMapping::ParseBracketFormID("ExampleQuest").has_value(), "plain EDID parsed as FormID");
	FallHookTestSupport::require(XmlTranslationMapping::GetTranslationType("WEAP FULL") == TranslationType::kFullName, "WEAP FULL mapping failed");
	FallHookTestSupport::require(XmlTranslationMapping::GetTranslationType("NPC_ FULL") == TranslationType::kNpcFullName, "NPC FULL mapping failed");
	FallHookTestSupport::require(XmlTranslationMapping::GetTranslationType("INFO NAM1") == TranslationType::kRuntimeIndex, "INFO NAM1 mapping failed");
	FallHookTestSupport::require(XmlTranslationMapping::GetTranslationType("PERK EPF2") == TranslationType::kButtonText2, "PERK EPF2 mapping failed");
	FallHookTestSupport::require(XmlTranslationMapping::GetTranslationType("TERM RNAM") == TranslationType::kTerminalResponseText, "TERM RNAM mapping failed");
	FallHookTestSupport::require(XmlTranslationMapping::GetTranslationType("TES4 SNAM") == TranslationType::kUnknown, "unsupported signature should be unknown");
}

void testRuntimeResolution()
{
	XmlTranslationEntry entry;
	entry.recordOrdinal = 2;
	FallHookTestSupport::require(!RuntimeResolution::GetSemanticIndex(entry, TranslationType::kRuntime1).has_value(), "runtime1 index should stay empty");
	FallHookTestSupport::require(RuntimeResolution::GetSemanticIndex(entry, TranslationType::kRuntimeLegacy) == 2, "runtime legacy should use ordinal");
	entry.index = 5;
	FallHookTestSupport::require(RuntimeResolution::GetSemanticIndex(entry, TranslationType::kRuntimeIndex) == 5, "runtime index should prefer REC id");
	FallHookTestSupport::require(RuntimeResolution::GetSemanticIndex(entry, TranslationType::kFullName) == 5, "indexed item FULL should preserve REC id");
	FallHookTestSupport::require(RuntimeResolution::GetSemanticIndex(entry, TranslationType::kNpcFullName) == 5, "indexed NPC FULL should preserve REC id");
}

void testLoadOrder()
{
	std::vector<XmlLoadOrder::SortEntry> entries{
		{ 0, "Fallout4.xml", "Fallout4.esm", 0 },
		{ 1, "DLCNukaWorld.xml", "DLCNukaWorld.esm", 6 },
		{ 2, "DLCRobot.xml", "DLCRobot.esm", 1 }
	};

	auto order = XmlLoadOrder::SortIndices(entries, XmlLoadOrder::Mode::kPlugin);
	FallHookTestSupport::require(order[0] == 1 && order[2] == 0, "plugin priority sort mismatch");
}

void testSourceFreeKey()
{
	SourceFreeTranslationKey key;
	key.pluginName = " Example.ESP ";
	key.formID = 0x01001234;
	key.editorID = " QuestObjective ";
	key.type = TranslationType::kQuestObjective;
	key.index = 2;
	key.stringID = 0xABC;
	const auto built = SourceFreeTranslationKeys::MakeKey(key);
	FallHookTestSupport::require(built == "p=example.esp|f=01001234|e=questobjective|t=7|i=2|sid=2748", "source-free key mismatch");
}
