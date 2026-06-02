// AI CONTEXT: Installs the TESQuestStageItem log-entry hook for QUST:CNAM translation.
// Depends on TESQuestStageItem::GetLogEntry and RuntimeQuestLogTranslations.
// Runtime scope is Fallout 4 1.11.191 quest journal stage/item text support.
// Version-specific logic: fixed 1.11.191 GetLogEntry hook ID.
// Source-free policy: resolves by quest FormID plus stage/item-derived CNAM index; never Source text.
#pragma once

namespace Runtime111191::RuntimeQuestJournalTextHook
{
	void Install();
}
