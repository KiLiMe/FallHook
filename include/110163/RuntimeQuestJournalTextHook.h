// AI CONTEXT: Installs the TESQuest stage/item journal text hook for QUST:CNAM translation.
// Depends on TESQuest::GetJournalTextForStageItem and RuntimeQuestLogTranslations.
// Runtime scope is Fallout 4 1.10.163 Address Library ID 139753 only.
// Version-specific logic: fixed 1.10.163 quest journal text hook address.
// Source-free policy: resolves by quest FormID plus stage/item-derived CNAM index; never Source text.
#pragma once

namespace RuntimeQuestJournalTextHook
{
	void Install();
}
