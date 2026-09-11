// AI CONTEXT: Implements loaded dialogue form collection from the global TESForm registry.
// Depends on CommonLibF4 TESTopic/TESForm containers.
// Runtime scope is Fallout 4 1.11.240 loaded dialogue data enumeration only.
// Version-specific logic: none beyond CommonLibF4 Fallout 4 form layout assumptions.
// Source-free policy: collects data objects by pointer identity only; never reads Source or visible text.
#include "PCH.h"

#include "111240/RuntimeDialogueForms.h"

#include "RE/T/TESForm.h"
#include "RE/T/TESTopic.h"

namespace Runtime111240
{
namespace RuntimeDialogueForms
{
	LoadedForms Collect()
	{
		LoadedForms forms;

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
				if (auto* topic = form->As<RE::TESTopic>())
				{
					forms.topics.push_back(topic);
				}
			}
		}

		return forms;
	}
}

} // namespace Runtime111240
