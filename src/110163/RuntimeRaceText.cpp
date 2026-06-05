// AI CONTEXT: Writes destination text into nested TESRace localized-name fields.
// Depends on RuntimeLocalizedStringID, RuntimeTextStringAssign, and CommonLibF4 TESRace face data.
// Runtime scope is Fallout 4 1.10.163 RACE:FMRN, RACE:MPPN, and RACE:TTGP data.
// Version-specific logic: Fallout 4 1.10.163 TESRace::faceRelatedData layout and xEdit flat REC ordering.
// Source-free policy: resolves slots by sID/index identity and writes Dest only; original Source text is never read.
#include "PCH.h"

#include "110163/RuntimeLocalizedStringID.h"
#include "110163/RuntimeRaceText.h"
#include "110163/RuntimeTextStringAssign.h"

namespace
{
	template <class Enumerate>
	RE::BGSLocalizedString* findSlot(
		const SourceFreeTranslationData& data,
		Enumerate&& enumerate)
	{
		RE::BGSLocalizedString* indexedSlot = nullptr;
		RE::BGSLocalizedString* stringIDSlot = nullptr;
		std::uint32_t position = 0;

		enumerate([&](RE::BGSLocalizedString& slot) {
			if (data.index && position == *data.index)
			{
				indexedSlot = std::addressof(slot);
			}
			if (data.stringID && RuntimeLocalizedStringID::Read(slot) == data.stringID)
			{
				stringIDSlot = std::addressof(slot);
			}
			++position;
		});

		return stringIDSlot ? stringIDSlot : indexedSlot;
	}

	template <class Visitor>
	void forEachFacialRegionName(RE::TESRace& race, Visitor&& visitor)
	{
		for (auto* faceData : race.faceRelatedData)
		{
			if (!faceData || !faceData->facialBoneRegions)
			{
				continue;
			}
			for (auto* region : *faceData->facialBoneRegions)
			{
				if (region)
				{
					visitor(region->name);
				}
			}
		}
	}

	template <class Visitor>
	void forEachMorphPresetName(RE::TESRace& race, Visitor&& visitor)
	{
		for (auto* faceData : race.faceRelatedData)
		{
			if (!faceData || !faceData->morphGroups)
			{
				continue;
			}
			for (auto* group : *faceData->morphGroups)
			{
				if (!group)
				{
					continue;
				}
				for (auto& preset : group->presets)
				{
					visitor(preset.name);
				}
			}
		}
	}

	template <class Visitor>
	void forEachTintTemplateName(RE::TESRace& race, Visitor&& visitor)
	{
		for (auto* faceData : race.faceRelatedData)
		{
			auto* templateGroups = faceData ? faceData->tintingTemplate : nullptr;
			if (!templateGroups)
			{
				continue;
			}
			for (auto* group : templateGroups->groups)
			{
				if (!group)
				{
					continue;
				}

				visitor(group->name);
				for (auto* entry : group->entries)
				{
					if (entry)
					{
						visitor(entry->name);
					}
				}
			}
		}
	}

	RE::BGSLocalizedString* resolveSlot(RE::TESRace& race, const SourceFreeTranslationData& data)
	{
		switch (data.translationType)
		{
		case TranslationType::kRaceMorphRegionName:
			return findSlot(data, [&](auto&& visitor) {
				forEachFacialRegionName(race, visitor);
			});
		case TranslationType::kRaceMorphPresetName:
			return findSlot(data, [&](auto&& visitor) {
				forEachMorphPresetName(race, visitor);
			});
		case TranslationType::kRaceTintGroupName:
			return findSlot(data, [&](auto&& visitor) {
				forEachTintTemplateName(race, visitor);
			});
		default:
			return nullptr;
		}
	}
}

namespace RuntimeRaceText
{
	bool Apply(RE::TESForm* form, const SourceFreeTranslationData& data)
	{
		if (data.replacerText.empty() || (!data.stringID && !data.index))
		{
			return false;
		}

		auto* race = form ? form->As<RE::TESRace>() : nullptr;
		auto* slot = race ? resolveSlot(*race, data) : nullptr;
		if (!slot)
		{
			return false;
		}

		RuntimeTextStringAssign::AssignLocalized(*slot, data.replacerText);
		return true;
	}
}
