// AI CONTEXT: Stores source-free INFO:NAM1 subtitle fallback text for 1.10.163.
// Depends on TranslationCatalog and CommonLibF4 fixed strings/topic-info declarations.
// Runtime scope is Fallout 4 1.10.163 subtitle text before HUD subtitle display.
// Version-specific logic: resolves 1.10.163 INFO form IDs during Rebuild.
// Source-free policy: resolves by INFO form and sID metadata only; never by XML Source or live sentence text.
#pragma once

#include "TranslationCatalog.h"

#include "RE/B/BSFixedString.h"

#include <cstdint>
#include <string_view>

namespace RE
{
	class TESTopicInfo;
}

namespace RuntimeDialogueSubtitleTranslations
{
	struct LookupResult
	{
		RE::BSFixedStringCS text;
		std::string_view mode;
		std::uint32_t formID{ 0 };
		bool translated{ false };
	};

	void Rebuild(const TranslationCatalogBuildResult& catalog, bool force = false);
	[[nodiscard]] LookupResult Resolve(RE::TESTopicInfo* topicInfo, std::string_view liveText);
}
