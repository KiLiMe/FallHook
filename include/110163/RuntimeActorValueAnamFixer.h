// AI CONTEXT: Applies AVIF:ANAM destination text into ActorValueInfo abbreviation data.
// Depends on CommonLibF4 ActorValueInfo through the plugin target and source-free translation data.
// Runtime scope is Fallout 4 1.10.163 AVIF:ANAM actor value abbreviation fields only.
// Version-specific logic: Fallout 4 1.10.163 AVIF ANAM offset 0x160; no alternate runtime branches.
// Source-free policy: writes Dest into resolved AVIF ANAM data only; never reads Source text.
#pragma once

#include "Shared.h"

namespace RE
{
	class TESForm;
}

namespace RuntimeActorValueAnamFixer
{
	[[nodiscard]] bool Apply(RE::TESForm* form, const SourceFreeTranslationData& data);
}
