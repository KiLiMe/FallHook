// AI CONTEXT: Stores source-free dialogue choice prompt lookup data for 1.10.163 button hooks.
// Depends on TranslationCatalog and fixed-size form identity context structs.
// Runtime scope is Fallout 4 1.10.163 INFO:RNAM and DIAL:FULL dialogue choice lookup.
// Version-specific logic: uses 110163 form resolution during Rebuild.
// Source-free policy: resolves by INFO/DIAL form identity only; visible text and XML Source are never lookup keys.
#pragma once

#include "TranslationCatalog.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace RuntimeDialogueChoiceTranslations
{
	constexpr std::size_t kMaxChoiceInfos{ 4 };

	struct ChoiceContext
	{
		std::array<std::uint32_t, kMaxChoiceInfos> infoFormIDs{};
		std::size_t infoCount{ 0 };
		std::uint32_t topicFormID{ 0 };
		std::uint32_t questFormID{ 0 };
		std::uint32_t responseType{ 0 };
	};

	struct LookupResult
	{
		std::string text;
		std::string_view mode;
		std::uint32_t formID{ 0 };
		bool translated{ false };
	};

	void Rebuild(const TranslationCatalogBuildResult& catalog, bool force);
	[[nodiscard]] LookupResult Resolve(const ChoiceContext& context);
}
