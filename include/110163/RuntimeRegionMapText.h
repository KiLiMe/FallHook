// AI CONTEXT: Writes source-free REGN:RDMP destination text into region map-data storage.
// Depends on SourceFreeTranslationData and a focused Fallout 4 region-data writer implementation.
// Runtime scope is Fallout 4 1.10.163 region transition/map rollover labels.
// Version-specific logic: Fallout 4 1.10.163 TESRegionDataMap layout and resolver only.
// Source-free policy: resolves the REGN form/map slot and writes Dest; never matches Source text.
#pragma once

#include "Shared.h"

namespace RE
{
	class TESForm;
}

namespace RuntimeRegionMapText
{
	[[nodiscard]] bool Apply(RE::TESForm* form, const SourceFreeTranslationData& data);
}
