// AI CONTEXT: Stores and applies source-free INFO:NAM1 response translations for constructed dialogue responses.
// Depends on TranslationCatalog, CommonLibF4 TESTopicInfo/DialogueResponse, and 1.10.163 form resolution.
// Runtime scope is Fallout 4 1.10.163 dialogue response construction before subtitle/display use.
// Version-specific logic: uses 110163 resolver/string assignment modules; no hook addresses live here.
// Source-free policy: matches by INFO form, response sID, ordinal, and response ID candidates; never by Source text.
#pragma once

#include "TranslationCatalog.h"

namespace RE
{
	class DialogueResponse;
	class TESTopicInfo;
	class TESResponse;
}

namespace RuntimeDialogueResponseTranslations
{
	void Rebuild(const TranslationCatalogBuildResult& catalog, bool force = false);
	[[nodiscard]] bool ApplyConstructedResponse(
		RE::DialogueResponse* constructed,
		RE::TESTopicInfo* topicInfo,
		RE::TESResponse* response);
}
