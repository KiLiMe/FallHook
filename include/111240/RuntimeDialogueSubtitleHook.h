// AI CONTEXT: Declares the 1.11.240 SubtitleManager::ShowSubtitle identity fallback hook.
// Depends on RuntimeDialogueSubtitleTranslations and RuntimePrologueHook.
// Runtime scope is Fallout 4 1.11.240 subtitles carrying TESTopicInfo identity.
// Version-specific logic: implementation owns verified 1.11.240 Address Library ID 2249542.
// Source-free policy: hook delegates to INFO form/sID lookup; it never matches visible/source text.
#pragma once

namespace Runtime111240::RuntimeDialogueSubtitleHook
{
	void Install();
}
