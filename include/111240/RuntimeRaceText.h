// AI CONTEXT: Applies source-free RACE morph-region, morph-preset, and tint-template names.
// Depends on CommonLibF4 TESRace nested face-data containers through the plugin target.
// Runtime scope is Fallout 4 1.11.240 direct RACE data mutation only.
// Version-specific logic: Fallout 4 1.11.240 TESRace layout and nested container order.
// Source-free policy: selects live slots by XML sID first and verified REC index second; never matches Source text.
#pragma once

#include "Shared.h"

namespace RE
{
	class TESForm;
}

namespace Runtime111240::RuntimeRaceText
{
	[[nodiscard]] bool Apply(RE::TESForm* form, const SourceFreeTranslationData& data);
}
