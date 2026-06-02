// AI CONTEXT: Applies REGN:RDMP destination text to the TESRegionDataMap name slot.
// Depends on RuntimeTextStringAssign, CommonLibF4 TESRegion/TESRegionData, and Address Library ID 657534.
// Runtime scope is Fallout 4 1.10.163 region labels consumed by HUD rollover/cell transitions.
// Version-specific logic: TESRegionDataList::Find ID 657534 and TESRegionDataMap::mapName offset 0x10.
// Source-free policy: locates data by resolved REGN form plus RDMP slot identity and writes Dest only.
#include "PCH.h"

#include "110163/RuntimeRegionMapText.h"
#include "110163/RuntimeTextStringAssign.h"

#include <cstddef>

namespace
{
	constexpr REL::ID kFindRegionDataID{ 657534 };
	constexpr std::ptrdiff_t kMapNameOffset = 0x10;

	using FindRegionData_t = RE::TESRegionData*(RE::TESRegionDataList*, RE::REGION_DATA_ID);

	RE::TESRegionData* findMapData(RE::TESRegionDataList* list)
	{
		if (!list)
		{
			return nullptr;
		}

		static REL::Relocation<FindRegionData_t> findRegionData{ kFindRegionDataID };
		return findRegionData(list, RE::REGION_DATA_ID::kMapID);
	}

	RE::BGSLocalizedString* mapNameSlot(RE::TESRegionData* mapData) noexcept
	{
		auto* bytes = reinterpret_cast<std::byte*>(mapData);
		return reinterpret_cast<RE::BGSLocalizedString*>(bytes + kMapNameOffset);
	}
}

namespace RuntimeRegionMapText
{
	bool Apply(RE::TESForm* form, const SourceFreeTranslationData& data)
	{
		if (data.translationType != TranslationType::kRegion || data.replacerText.empty())
		{
			return false;
		}

		auto* region = form ? form->As<RE::TESRegion>() : nullptr;
		auto* mapData = region ? findMapData(region->dataList) : nullptr;
		if (!mapData || mapData->GetID() != RE::REGION_DATA_ID::kMapID)
		{
			return false;
		}

		RuntimeTextStringAssign::AssignPlainLocalized(*mapNameSlot(mapData), data.replacerText);
		return true;
	}
}
