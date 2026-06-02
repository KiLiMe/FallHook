// AI CONTEXT: Declares the 1.11.191 DialogueResponse constructor hook.
// Depends on RuntimeDialogueResponseTranslations through the implementation.
// Runtime scope is Fallout 4 1.11.191 dialogue response construction only.
// Version-specific logic: implementation owns the guarded 1.11.191 constructor offset fallback.
// Source-free policy: hook only triggers source-free response maps; no visible/source text lookup.
#pragma once

namespace Runtime111191::RuntimeDialogueResponseHook
{
	void Install();
}
