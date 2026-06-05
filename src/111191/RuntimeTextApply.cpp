// AI CONTEXT: Dispatches source-free const text records to focused runtime writer modules.
// Depends on RuntimeText* writer modules, RuntimeTextStringAssign, and CommonLibF4 game settings.
// Runtime scope is Fallout 4 1.11.191 direct non-hook text mutation.
// Version-specific logic: Fallout 4 1.11.191 only, including verified REGN:RDMP map-data mutation.
// Source-free policy: only routes resolved destination data; no original text lookup or fallback exists.
#include "PCH.h"

#include "111191/RuntimeTextApply.h"
#include "111191/RuntimeTextIndexedLists.h"
#include "111191/RuntimeRaceText.h"
#include "111191/RuntimeRegionMapText.h"
#include "111191/RuntimeTextReference.h"
#include "111191/RuntimeActorValueAnamFixer.h"
#include "111191/RuntimeTextSimpleFields.h"
#include "111191/RuntimeTextStringAssign.h"
#include "111191/RuntimeTextTerminal.h"
#include "MessageIconFormatter.h"

namespace Runtime111191
{
namespace RuntimeTextApply
{
	bool IsAllowed(TranslationType type) noexcept
	{
		switch (type)
		{
		case TranslationType::kFullName:
		case TranslationType::kLocationFullName:
		case TranslationType::kLoadScreenDescription:
		case TranslationType::kMagicDescription:
		case TranslationType::kShortName:
		case TranslationType::kWordOfPower:
		case TranslationType::kGameSetting:
		case TranslationType::kRegion:
		case TranslationType::kQuestObjective:
		case TranslationType::kButtonText1:
		case TranslationType::kBodyPartName:
		case TranslationType::kFactionMaleRank:
		case TranslationType::kFactionFemaleRank:
		case TranslationType::kAlchemyAddictionName:
		case TranslationType::kAmmoShortDescription:
		case TranslationType::kDoorAlternateOpenText:
		case TranslationType::kDoorAlternateCloseText:
		case TranslationType::kMessageShortName:
		case TranslationType::kActorValueAbbreviation:
		case TranslationType::kRaceMorphRegionName:
		case TranslationType::kRaceMorphPresetName:
		case TranslationType::kRaceTintGroupName:
		case TranslationType::kReference:
		case TranslationType::kTerminalResultText:
		case TranslationType::kTerminalBodyText:
		case TranslationType::kTerminalItemText:
		case TranslationType::kTerminalResponseText:
		case TranslationType::kTerminalHeaderText:
		case TranslationType::kTerminalWelcomeText:
			return true;
		default:
			return false;
		}
	}

	bool ApplyGameSetting(const SourceFreeTranslationData& data)
	{
		if (!data.editorID || data.editorID->empty())
		{
			return false;
		}
		return ApplyGameSetting(*data.editorID, data.replacerText);
	}

	bool ApplyGameSetting(std::string_view editorID, std::string_view text)
	{
		if (editorID.empty())
		{
			return false;
		}
		const auto collection = RE::GameSettingCollection::GetSingleton();
		auto* setting = collection ? collection->GetSetting(editorID) : nullptr;
		if (!setting || setting->GetType() != RE::Setting::SETTING_TYPE::kString)
		{
			return false;
		}

		const auto formatted = MessageIconFormatter::ApplyRuntimeControlMarkup(setting->GetString(), text);
		const auto value = RuntimeTextStringAssign::NonEmptyString(formatted.text);
		setting->SetString(_strdup(value.c_str()));
		return true;
	}

	bool ApplyForm(RE::TESForm* form, const SourceFreeTranslationData& data)
	{
		if (!IsAllowed(data.translationType))
		{
			return false;
		}

		switch (data.translationType)
		{
		case TranslationType::kFullName:
		case TranslationType::kLocationFullName:
			return RuntimeTextSimpleFields::ApplyFullName(form, data);
		case TranslationType::kLoadScreenDescription:
			return RuntimeTextSimpleFields::ApplyLoadScreenDescription(form, data);
		case TranslationType::kMagicDescription:
			return RuntimeTextSimpleFields::ApplyMagicDescription(form, data);
		case TranslationType::kShortName:
			return RuntimeTextSimpleFields::ApplyShortName(form, data);
		case TranslationType::kWordOfPower:
			return RuntimeTextSimpleFields::ApplyWordOfPower(form, data);
		case TranslationType::kAlchemyAddictionName:
			return RuntimeTextSimpleFields::ApplyAlchemyAddictionName(form, data);
		case TranslationType::kAmmoShortDescription:
			return RuntimeTextSimpleFields::ApplyAmmoShortDescription(form, data);
		case TranslationType::kDoorAlternateOpenText:
			return RuntimeTextSimpleFields::ApplyDoorAlternateOpenText(form, data);
		case TranslationType::kDoorAlternateCloseText:
			return RuntimeTextSimpleFields::ApplyDoorAlternateCloseText(form, data);
		case TranslationType::kMessageShortName:
			return RuntimeTextSimpleFields::ApplyMessageShortName(form, data);
		case TranslationType::kRegion:
			return RuntimeRegionMapText::Apply(form, data);
		case TranslationType::kActorValueAbbreviation:
			return RuntimeActorValueAnamFixer::Apply(form, data);
		case TranslationType::kRaceMorphRegionName:
		case TranslationType::kRaceMorphPresetName:
		case TranslationType::kRaceTintGroupName:
			return RuntimeRaceText::Apply(form, data);
		case TranslationType::kButtonText1:
			return RuntimeTextIndexedLists::ApplyMessageBoxButton(form, data);
		case TranslationType::kQuestObjective:
			return RuntimeTextIndexedLists::ApplyQuestObjective(form, data);
		case TranslationType::kBodyPartName:
			return RuntimeTextIndexedLists::ApplyBodyPartName(form, data);
		case TranslationType::kFactionMaleRank:
			return RuntimeTextIndexedLists::ApplyFactionRankTitle(form, data, false);
		case TranslationType::kFactionFemaleRank:
			return RuntimeTextIndexedLists::ApplyFactionRankTitle(form, data, true);
		case TranslationType::kReference:
			return RuntimeTextReference::ApplyReference(form, data);
		case TranslationType::kTerminalResultText:
		case TranslationType::kTerminalBodyText:
		case TranslationType::kTerminalItemText:
		case TranslationType::kTerminalResponseText:
		case TranslationType::kTerminalHeaderText:
		case TranslationType::kTerminalWelcomeText:
			return RuntimeTextTerminal::ApplyTerminalText(form, data);
		default:
			return false;
		}
	}
}

} // namespace Runtime111191
