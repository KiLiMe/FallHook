// AI CONTEXT: Writes source-free destination strings into indexed runtime list fields, including body-part names.
// Depends on SourceFreeTranslationData indexes, MESG icon markup preservation, and CommonLibF4 list-backed record types.
// Runtime scope is Fallout 4 1.11.191 message, quest, body part, and faction rank fields.
// Version-specific logic: Fallout 4 1.11.191 only; no alternate runtime branches.
// Source-free policy: target selection uses stable indexes and form identity; MESG runtime text only preserves markup.
// Exception note: MESG icon preservation is approved and must not be treated as source-exact lookup.
#pragma once

#include "Shared.h"

namespace RE
{
	class TESForm;
}

namespace Runtime111191::RuntimeTextIndexedLists
{
	[[nodiscard]] bool ApplyMessageBoxButton(RE::TESForm* form, const SourceFreeTranslationData& data);
	[[nodiscard]] bool ApplyQuestObjective(RE::TESForm* form, const SourceFreeTranslationData& data);
	[[nodiscard]] bool ApplyBodyPartName(RE::TESForm* form, const SourceFreeTranslationData& data);
	[[nodiscard]] bool ApplyFactionRankTitle(RE::TESForm* form, const SourceFreeTranslationData& data, bool female);
}
