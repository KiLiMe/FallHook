// AI CONTEXT: Builds direct non-hook runtime apply maps from the source-free catalog.
// Depends only on TranslationCatalog data and source-free key normalization.
// Runtime assumptions: version-neutral Fallout 4 const text categories.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: entries carry destination text and identity only, never original Source text.
#pragma once

#include "TranslationCatalog.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct ConstApplyEntry
{
	SourceFreeTranslationData data;
	std::string pluginName;
	std::string recordSignature;
};

struct ConstApplyBuildStats
{
	std::size_t totalRecords{ 0 };
	std::size_t formEntries{ 0 };
	std::size_t editorIDFormEntries{ 0 };
	std::size_t gameSettingEntries{ 0 };
	std::size_t skippedEmptyText{ 0 };
	std::size_t skippedUnsupportedType{ 0 };
	std::size_t skippedMissingTarget{ 0 };
};

struct ConstApplyMaps
{
	std::unordered_multimap<std::uint32_t, ConstApplyEntry> formEntries;
	std::vector<ConstApplyEntry> editorIDFormEntries;
	std::unordered_map<std::string, ConstApplyEntry> gameSettingEntries;
	ConstApplyBuildStats stats;
};

namespace ConstApplyMap
{
	[[nodiscard]] bool IsDirectConstApplyType(TranslationType type) noexcept;
	[[nodiscard]] ConstApplyMaps Build(const TranslationCatalogBuildResult& catalog);
}
