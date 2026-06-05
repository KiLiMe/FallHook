// AI CONTEXT: Installs the 1.10.163 SubtitleManager::ShowSubtitle identity fallback hook.
// Depends on RuntimeDialogueSubtitleTranslations and RuntimePrologueHook.
// Runtime scope is Fallout 4 1.10.163 subtitles carrying TESTopicInfo identity.
// Version-specific logic: implementation owns the verified 1.10.163 Address Library ID.
// Source-free policy: hook delegates to INFO form/sID lookup; it never matches visible/source text.
#pragma once

namespace RuntimeDialogueSubtitleHook
{
	void Install();
}
