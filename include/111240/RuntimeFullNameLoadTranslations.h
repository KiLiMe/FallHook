// AI CONTEXT: Holds source-free FULL translations for TESFullName load-time mutation.
// Depends on TranslationCatalog and CommonLibF4 TESForm declarations.
// Runtime scope is Fallout 4 1.11.240 TESFullName::LoadFullNameChunk hook support.
// Version-specific logic: none here; hook site is isolated in RuntimeFullNameLoadHook.
// Source-free policy: maps by resolved FormID, editor ID, or runtime sID; XML Source is ignored.
#pragma once

#include "TranslationCatalog.h"

#include <cstdint>
#include <string>
#include <optional>

namespace RE
{
	class TESForm;
}

namespace Runtime111240::RuntimeFullNameLoadTranslations
{
	struct RebuildStats
	{
		std::size_t accepted{ 0 };
		std::size_t editorEntries{ 0 };
		std::size_t formEntries{ 0 };
		std::size_t sidEntries{ 0 };
		std::size_t indexedBaseEntries{ 0 };
		std::size_t skippedWrongType{ 0 };
		std::size_t skippedUnsupportedSignature{ 0 };
		std::size_t skippedIndexed{ 0 };
		std::size_t skippedEmptyText{ 0 };
		std::size_t skippedMissingTarget{ 0 };
		std::size_t skippedUnresolvedFormID{ 0 };
	};

	struct IndexedFallback
	{
		std::uint32_t index{ 0 };
		std::string text;
	};

	enum class LookupSource : std::uint8_t
	{
		kNone,
		kStringID,
		kFormID,
		kEditorID,
		kNpcIndexedFallback
	};

	struct LookupResult
	{
		const std::string* text{ nullptr };
		LookupSource source{ LookupSource::kNone };
		std::uint32_t index{ 0xFFFFFFFFu };
	};

	RebuildStats Rebuild(const TranslationCatalogBuildResult& catalog, bool force = false);
	[[nodiscard]] bool MayNeedOwnerLookup(std::optional<std::uint32_t> stringID) noexcept;
	[[nodiscard]] LookupResult Lookup(const RE::TESForm* form, std::optional<std::uint32_t> stringID);
}
