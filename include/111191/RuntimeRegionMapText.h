// AI CONTEXT: Writes source-free REGN:RDMP destination text into region map-data storage.
// Depends on SourceFreeTranslationData and CommonLibF4 form declarations.
// Runtime scope is Fallout 4 1.11.191 region transition/map rollover labels.
// Version-specific logic: TESRegionDataList::Find ID 2196228 and TESRegionDataMap::mapName offset 0x10.
// Source-free policy: locates data by resolved REGN form plus RDMP slot identity and writes Dest only.
#pragma once

#include "Shared.h"

namespace RE
{
	class TESForm;
}

namespace Runtime111191::RuntimeRegionMapText
{
	[[nodiscard]] bool Apply(RE::TESForm* form, const SourceFreeTranslationData& data);
}
