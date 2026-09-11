// AI CONTEXT: Applies REGN:RDMP destination text to the TESRegionDataMap name slot.
// Depends on RuntimeTextStringAssign, CommonLibF4 TESRegion/TESRegionData, and Address Library ID 2196228.
// Runtime scope is Fallout 4 1.11.240 region labels consumed by HUD rollover/cell transitions.
// Version-specific logic: TESRegionDataList::Find ID 2196228 and TESRegionDataMap::mapName offset 0x10.
// Source-free policy: locates data by resolved REGN form plus RDMP slot identity and writes Dest only.
#include "PCH.h"

#include "111240/RuntimeRegionMapText.h"
#include "111240/RuntimeTextStringAssign.h"

#include <cstddef>

namespace
{
	constexpr REL::ID kFindRegionDataID{ 2196228 };
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

namespace Runtime111240
{
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

} // namespace Runtime111240
