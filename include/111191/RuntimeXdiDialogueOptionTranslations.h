// AI CONTEXT: Stores 1.11.191 XDI dialogue option text by source-free INFO identity.
// Depends on TranslationCatalog records and 1.11.191 runtime form resolution.
// Runtime scope is XDI DialogueMenu prompt/response replacement only.
// Version-specific logic: resolves 1.11.191 runtime INFO form IDs from XML/plugin identity.
// Source-free policy: resolves by INFO form ID; visible text and XML Source are never lookup keys.
#pragma once

#include "TranslationCatalog.h"

#include <cstdint>
#include <string>

namespace Runtime111191::RuntimeXdiDialogueOptionTranslations
{
	struct OptionText
	{
		std::string prompt;
		std::string response;
		std::uint32_t infoFormID{ 0 };
		bool hasPrompt{ false };
		bool hasResponse{ false };
	};

	void Rebuild(const TranslationCatalogBuildResult& catalog, bool force = false);
	[[nodiscard]] OptionText Resolve(std::uint32_t infoFormID);
}
