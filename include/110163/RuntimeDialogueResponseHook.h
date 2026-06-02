// AI CONTEXT: Installs the 1.10.163 DialogueResponse constructor hook for INFO:NAM1 spoken-line mutation.
// Depends on RuntimeDialogueResponseTranslations and RuntimePrologueHook.
// Runtime scope is Fallout 4 1.10.163 dialogue response construction only.
// Version-specific logic: implementation owns the verified 1.10.163 DialogueResponse Address Library ID.
// Source-free policy: hook only triggers source-free response translation maps; no visible/source text lookup.
#pragma once

namespace RuntimeDialogueResponseHook
{
	void Install();
}
