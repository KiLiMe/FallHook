// AI CONTEXT: Installs the TESQuestStageItem log-entry hook for QUST:CNAM translation.
// Depends on TESQuestStageItem::GetLogEntry and RuntimeQuestLogTranslations.
// Runtime scope is Fallout 4 1.11.240 quest journal stage/item text support.
// Version-specific logic: fixed 1.11.240 GetLogEntry hook ID.
// Source-free policy: resolves by quest FormID plus stage/item-derived CNAM index; never Source text.
#pragma once

namespace Runtime111240::RuntimeQuestJournalTextHook
{
	void Install();
}
