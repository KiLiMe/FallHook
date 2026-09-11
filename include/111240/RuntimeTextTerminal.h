// AI CONTEXT: Writes source-free destination strings into terminal body/menu text fields.
// Depends on SourceFreeTranslationData terminal indexes/string IDs and CommonLibF4 BGSTerminal structures.
// Runtime scope is Fallout 4 1.11.240 terminal text fields.
// Version-specific logic: Fallout 4 1.11.240 only; no alternate runtime branches.
// Source-free policy: selects by terminal sID/subrecord-ordinal identity only; never matches original text.
#pragma once

#include "Shared.h"

namespace RE
{
	class TESForm;
}

namespace Runtime111240::RuntimeTextTerminal
{
	[[nodiscard]] bool ApplyTerminalText(RE::TESForm* form, const SourceFreeTranslationData& data);
}
