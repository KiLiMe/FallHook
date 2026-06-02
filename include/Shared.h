// AI CONTEXT: Shared translation data types for the source-free core.
// Depends only on the C++ standard library so parser tests stay independent of F4SE.
// Runtime assumptions: version-neutral Fallout 4 data semantics.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: these types store destination identity fields, not original-text keys.
#pragma once

#include <cstdint>
#include <optional>
#include <string>

enum class TranslationType : std::uint8_t
{
	kFullName,
	kShortName,
	kWordOfPower,
	kGameSetting,
	kRegion,
	kMagicDescription,
	kLoadScreenDescription,
	kQuestObjective,
	kButtonText1,
	kButtonText2,
	kActivationText,
	kReference,
	kPerkVerb,
	kBodyPartName,
	kFactionMaleRank,
	kFactionFemaleRank,
	kRuntime1,
	kRuntime2,
	kRuntimeIndex,
	kRuntimeLegacy,
	kInstanceNamePart,
	kTerminalResultText,
	kTerminalBodyText,
	kTerminalItemText,
	kTerminalResponseText,
	kTerminalHeaderText,
	kTerminalWelcomeText,
	kNpcFullName,
	kLocationFullName,
	kAlchemyAddictionName,
	kAmmoShortDescription,
	kActorValueAbbreviation,
	kDoorAlternateOpenText,
	kDoorAlternateCloseText,
	kMessageShortName,
	kLeveledListOverrideName,
	kRaceMorphRegionName,
	kRaceMorphPresetName,
	kRaceTintGroupName,

	kUnknown
};

struct SourceFreeTranslationData
{
	TranslationType translationType{ TranslationType::kUnknown };
	std::string replacerText;
	std::optional<std::uint32_t> formID;
	std::optional<std::uint32_t> index;
	std::optional<std::uint32_t> stringID;
	std::optional<std::string> editorID;
};
