// AI CONTEXT: Writes source-free destination strings into simple one-target form fields.
// Depends on CommonLibF4 form components and SourceFreeTranslationData destination text.
// Runtime scope is Fallout 4 1.11.191 direct FULL/DESC/name-like fields.
// Version-specific logic: Fallout 4 1.11.191 only; no alternate runtime branches.
// Source-free policy: only writes resolved destination fields; no original text lookup is performed.
#pragma once

#include "Shared.h"

namespace RE
{
	class TESForm;
}

namespace Runtime111191::RuntimeTextSimpleFields
{
	[[nodiscard]] bool ApplyFullName(RE::TESForm* form, const SourceFreeTranslationData& data);
	[[nodiscard]] bool ApplyLoadScreenDescription(RE::TESForm* form, const SourceFreeTranslationData& data);
	[[nodiscard]] bool ApplyMagicDescription(RE::TESForm* form, const SourceFreeTranslationData& data);
	[[nodiscard]] bool ApplyShortName(RE::TESForm* form, const SourceFreeTranslationData& data);
	[[nodiscard]] bool ApplyWordOfPower(RE::TESForm* form, const SourceFreeTranslationData& data);
	[[nodiscard]] bool ApplyAlchemyAddictionName(RE::TESForm* form, const SourceFreeTranslationData& data);
	[[nodiscard]] bool ApplyAmmoShortDescription(RE::TESForm* form, const SourceFreeTranslationData& data);
	[[nodiscard]] bool ApplyDoorAlternateOpenText(RE::TESForm* form, const SourceFreeTranslationData& data);
	[[nodiscard]] bool ApplyDoorAlternateCloseText(RE::TESForm* form, const SourceFreeTranslationData& data);
	[[nodiscard]] bool ApplyMessageShortName(RE::TESForm* form, const SourceFreeTranslationData& data);
}
