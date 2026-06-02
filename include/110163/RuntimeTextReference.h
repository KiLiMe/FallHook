// AI CONTEXT: Writes source-free destination strings into reference-owned runtime fields.
// Depends on CommonLibF4 reference extra data and SourceFreeTranslationData destination text.
// Runtime scope is Fallout 4 1.10.163 map marker reference text.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: mutates the resolved reference target only; never matches original text.
#pragma once

#include "Shared.h"

namespace RE
{
	class TESForm;
}

namespace RuntimeTextReference
{
	[[nodiscard]] bool ApplyReference(RE::TESForm* form, const SourceFreeTranslationData& data);
}
