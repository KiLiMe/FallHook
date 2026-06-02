// AI CONTEXT: Implements loaded dialogue form collection from the global TESForm registry.
// Depends on CommonLibF4 TESTopic/TESTopicInfo/TESForm containers.
// Runtime scope is Fallout 4 1.10.163 loaded dialogue data enumeration only.
// Version-specific logic: none beyond CommonLibF4 Fallout 4 form layout assumptions.
// Source-free policy: collects data objects by pointer identity only; never reads Source or visible text.
#include "PCH.h"

#include "110163/RuntimeDialogueForms.h"

#include "RE/T/TESForm.h"
#include "RE/T/TESTopic.h"
#include "RE/T/TESTopicInfo.h"

#include <unordered_set>

namespace
{
	void addInfo(
		RuntimeDialogueForms::LoadedForms& forms,
		std::unordered_set<RE::TESTopicInfo*>& seenInfos,
		RE::TESTopicInfo* info)
	{
		if (!info)
		{
			return;
		}
		if (!seenInfos.insert(info).second)
		{
			++forms.duplicateInfos;
			return;
		}
		forms.infos.push_back(info);
	}
}

namespace RuntimeDialogueForms
{
	LoadedForms Collect()
	{
		LoadedForms forms;
		std::unordered_set<RE::TESTopicInfo*> seenInfos;
		seenInfos.reserve(65536);

		const auto& [allForms, lock] = RE::TESForm::GetAllForms();
		if (!allForms)
		{
			return forms;
		}

		{
			const RE::BSAutoReadLock readLock{ lock };
			for (const auto& entry : *allForms)
			{
				auto* form = entry.second;
				if (!form)
				{
					continue;
				}
				++forms.scannedForms;
				if (auto* info = form->As<RE::TESTopicInfo>())
				{
					addInfo(forms, seenInfos, info);
					continue;
				}
				if (auto* topic = form->As<RE::TESTopic>())
				{
					forms.topics.push_back(topic);
				}
			}
		}

		return forms;
	}
}
