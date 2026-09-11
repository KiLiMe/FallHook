// AI CONTEXT: Applies AVIF:ANAM destination text into direct runtime data.
// Depends on CommonLibF4 form types, ActorValueInfo, and source-free translation data.
// Runtime scope is Fallout 4 1.11.240 AVIF abbreviation data.
// Version-specific logic: Fallout 4 1.11.240 AVIF ANAM offset 0x1A0.
// Source-free policy: writes Dest into resolved form data only; never reads Source text.
#pragma once

#include "Shared.h"

namespace RE
{
	class TESForm;
}

namespace Runtime111240::RuntimeActorValueAnamFixer
{
	[[nodiscard]] bool Apply(RE::TESForm* form, const SourceFreeTranslationData& data);
}
