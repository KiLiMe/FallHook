// AI CONTEXT: Installs 1.11.191 dialogue button and player-response context hooks.
// Depends on RuntimeDialogueChoiceTranslations and RuntimePrologueHook.
// Runtime scope is Fallout 4 1.11.191 dialogue menu player choice rendering.
// Version-specific logic: implementation owns verified 1.11.191 Address Library ID 2196825 for GetCurrentTopicInfo.
// Source-free policy: translates choices from cached INFO/DIAL identity context, not visible/source text.
#pragma once

namespace Runtime111191::RuntimeDialogueButtonHook
{
	void Install();
}
