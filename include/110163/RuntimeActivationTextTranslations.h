// AI CONTEXT: Stores source-free activation prompt translations for the activation data hook.
// Depends on TranslationCatalog records and CommonLibF4 form identity.
// Runtime scope is Fallout 4 1.10.163 ACTI/FLOR/FURN/NPC_ activation prompt records.
// Version-specific logic: none; HUDRollover owns runtime hook addresses.
// Source-free policy: maps by resolved form ID, editor ID, and sID; never by Source or live text.
#pragma once

#include "TranslationCatalog.h"

#include <optional>
#include <string>

namespace RE
{
	class TESForm;
}

namespace RuntimeActivationTextTranslations
{
	struct RebuildStats
	{
		std::size_t accepted{ 0 };
		std::size_t formEntries{ 0 };
		std::size_t editorEntries{ 0 };
		std::size_t sidEntries{ 0 };
		std::size_t knownFurnitureUseEntries{ 0 };
		std::size_t skippedWrongType{ 0 };
		std::size_t skippedEmptyText{ 0 };
		std::size_t skippedMissingTarget{ 0 };
		std::size_t skippedUnresolvedFormID{ 0 };
	};

	RebuildStats Rebuild(const TranslationCatalogBuildResult& catalog, bool force = false);
	[[nodiscard]] std::optional<std::string> Lookup(const RE::TESForm* form, std::optional<std::uint32_t> stringID);
	[[nodiscard]] std::optional<std::string> KnownFurnitureUseText();
}
