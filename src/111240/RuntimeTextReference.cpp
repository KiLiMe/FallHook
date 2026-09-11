// AI CONTEXT: Applies destination text to reference-owned map marker text.
// Depends on RuntimeTextReference declarations, RuntimeTextStringAssign, and CommonLibF4 reference extras.
// Runtime scope is Fallout 4 1.11.240 resolved reference text mutation.
// Version-specific logic: Fallout 4 1.11.240 only; no alternate runtime branches.
// Source-free policy: writes only the resolved reference slot; no original text matching occurs.
#include "PCH.h"

#include "111240/RuntimeTextReference.h"
#include "111240/RuntimeTextStringAssign.h"

namespace Runtime111240
{
namespace RuntimeTextReference
{
	bool ApplyReference(RE::TESForm* form, const SourceFreeTranslationData& data)
	{
		auto* ref = form ? form->As<RE::TESObjectREFR>() : nullptr;
		const auto extra = ref && ref->extraList ? ref->extraList->GetByType<RE::ExtraMapMarker>() : nullptr;
		auto* marker = extra ? extra->mapMarkerData : nullptr;
		if (!marker)
		{
			return false;
		}

		RuntimeTextStringAssign::AssignLocalized(static_cast<RE::TESFullName*>(marker)->fullName, data.replacerText);
		return true;
	}
}

} // namespace Runtime111240
