// AI CONTEXT: Installs 1.10.163 dialogue button and player-response context hooks.
// Depends on RuntimeDialogueChoiceTranslations and RuntimePrologueHook.
// Runtime scope is Fallout 4 1.10.163 dialogue menu player choice rendering.
// Version-specific logic: implementation owns verified 1.10.163 Address Library IDs 781358 and 643840.
// Source-free policy: translates choices from cached INFO/DIAL identity context, not visible/source text.
#pragma once

namespace RuntimeDialogueButtonHook
{
	void Install();
}
