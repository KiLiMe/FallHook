// AI CONTEXT: Stores and resolves source-free INFO:NAM1 translations for constructed dialogue responses.
// Depends on TranslationCatalog, CommonLibF4 dialogue response identity, and 1.11.240 form resolution.
// Runtime scope is Fallout 4 1.11.240 response construction before subtitle-context capture.
// Version-specific logic: uses 111240 resolver and dialogue layouts; no hook addresses live here.
// Source-free policy: matches by INFO form, response sID, ordinal, and response ID candidates; never by Source text.
#pragma once

#include "TranslationCatalog.h"

#include "RE/B/BSFixedString.h"

#include <cstdint>

namespace RE
{
	class TESTopic;
	class TESTopicInfo;
	class TESResponse;
}

namespace Runtime111240::RuntimeDialogueResponseTranslations
{
	struct LookupResult
	{
		RE::BSFixedStringCS text;
		std::uint32_t ordinal{ 0xFFFFFFFFu };
		std::uint32_t responseID{ 0 };
		bool translated{ false };
	};

	void Rebuild(const TranslationCatalogBuildResult& catalog, bool force = false);
	[[nodiscard]] LookupResult ResolveResponse(
		RE::TESTopic* topic,
		RE::TESTopicInfo* topicInfo,
		RE::TESResponse* response);
}
