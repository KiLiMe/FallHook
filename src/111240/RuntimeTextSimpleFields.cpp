// AI CONTEXT: Applies destination text to simple one-target runtime form fields.
// Depends on RuntimeTextSimpleFields declarations, RuntimeTextStringAssign, and CommonLibF4 form components.
// Runtime scope is Fallout 4 1.11.240 direct FULL/DESC/name-like mutations.
// Version-specific logic: Fallout 4 1.11.240 only; no alternate runtime branches.
// Source-free policy: consumes resolved forms plus destination text only; never matches original text.
#include "PCH.h"

#include "111240/RuntimeTextSimpleFields.h"
#include "111240/RuntimeTextStringAssign.h"

namespace Runtime111240
{
namespace
{
	bool supportsSparseFullName(RE::ENUM_FORM_ID formType)
	{
		switch (formType)
		{
		case RE::ENUM_FORM_ID::kKYWD:
		case RE::ENUM_FORM_ID::kLCRT:
		case RE::ENUM_FORM_ID::kAACT:
		case RE::ENUM_FORM_ID::kLIGH:
		case RE::ENUM_FORM_ID::kSTAT:
		case RE::ENUM_FORM_ID::kSCOL:
		case RE::ENUM_FORM_ID::kMSTT:
		case RE::ENUM_FORM_ID::kFLST:
			return true;
		default:
			return false;
		}
	}

	bool assignSparseFullName(RE::TESForm* form, std::string_view text)
	{
		if (!form || !supportsSparseFullName(form->GetFormType()))
		{
			return false;
		}

		auto& sparseNames = RE::TESFullName::GetSparseFullNameMap();
		auto it = sparseNames.find(form);
		if (it == sparseNames.end())
		{
			it = sparseNames.emplace(static_cast<const RE::TESForm*>(form), RE::BGSLocalizedString{}).first;
		}

		RuntimeTextStringAssign::AssignPlainLocalized(it->second, text);
		return true;
	}
}

namespace RuntimeTextSimpleFields
{
	bool ApplyFullName(RE::TESForm* form, const SourceFreeTranslationData& data)
	{
		if (auto* fullName = form ? form->As<RE::TESFullName>() : nullptr)
		{
			RuntimeTextStringAssign::AssignPlainLocalized(fullName->fullName, data.replacerText);
			return true;
		}
		return assignSparseFullName(form, data.replacerText);
	}

	bool ApplyLoadScreenDescription(RE::TESForm* form, const SourceFreeTranslationData& data)
	{
		if (auto* loadScreen = form ? form->As<RE::TESLoadScreen>() : nullptr)
		{
			RuntimeTextStringAssign::AssignLocalized(loadScreen->loadingText, data.replacerText);
			return true;
		}
		return false;
	}

	bool ApplyMagicDescription(RE::TESForm* form, const SourceFreeTranslationData& data)
	{
		if (auto* effect = form ? form->As<RE::EffectSetting>() : nullptr)
		{
			RuntimeTextStringAssign::AssignLocalized(effect->magicItemDescription, data.replacerText);
			return true;
		}
		return false;
	}

	bool ApplyShortName(RE::TESForm* form, const SourceFreeTranslationData& data)
	{
		if (auto* npc = form ? form->As<RE::TESNPC>() : nullptr)
		{
			RuntimeTextStringAssign::AssignLocalized(npc->shortName, data.replacerText);
			return true;
		}
		return false;
	}

	bool ApplyWordOfPower(RE::TESForm* form, const SourceFreeTranslationData& data)
	{
		if (auto* word = form ? form->As<RE::TESWordOfPower>() : nullptr)
		{
			RuntimeTextStringAssign::AssignLocalized(word->translation, data.replacerText);
			return true;
		}
		return false;
	}

	bool ApplyAlchemyAddictionName(RE::TESForm* form, const SourceFreeTranslationData& data)
	{
		if (auto* alchemy = form ? form->As<RE::AlchemyItem>() : nullptr)
		{
			RuntimeTextStringAssign::AssignLocalized(alchemy->data.addictionName, data.replacerText);
			return true;
		}
		return false;
	}

	bool ApplyAmmoShortDescription(RE::TESForm* form, const SourceFreeTranslationData& data)
	{
		if (auto* ammo = form ? form->As<RE::TESAmmo>() : nullptr)
		{
			RuntimeTextStringAssign::AssignLocalized(ammo->shortDesc, data.replacerText);
			return true;
		}
		return false;
	}

	bool ApplyDoorAlternateOpenText(RE::TESForm* form, const SourceFreeTranslationData& data)
	{
		if (auto* door = form ? form->As<RE::TESObjectDOOR>() : nullptr)
		{
			RuntimeTextStringAssign::AssignLocalized(door->altOpenText, data.replacerText);
			return true;
		}
		return false;
	}

	bool ApplyDoorAlternateCloseText(RE::TESForm* form, const SourceFreeTranslationData& data)
	{
		if (auto* door = form ? form->As<RE::TESObjectDOOR>() : nullptr)
		{
			RuntimeTextStringAssign::AssignLocalized(door->altCloseText, data.replacerText);
			return true;
		}
		return false;
	}

	bool ApplyMessageShortName(RE::TESForm* form, const SourceFreeTranslationData& data)
	{
		if (auto* message = form ? form->As<RE::BGSMessage>() : nullptr)
		{
			RuntimeTextStringAssign::AssignLocalized(message->shortName, data.replacerText);
			return true;
		}
		return false;
	}
}

} // namespace Runtime111240
