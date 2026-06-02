// AI CONTEXT: Caches and reapplies direct const text mutation maps for save-load refresh.
// Depends on ConstApplyMap, RuntimeApplySettings, and 1.10.163 runtime text writers.
// Runtime scope is Fallout 4 1.10.163 direct const text mutation only.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: cached entries use form/editor/sID/slot identity and never XML Source text.
#pragma once

#include "RuntimeApplySettings.h"
#include "TranslationCatalog.h"

#include <cstddef>
#include <string_view>

namespace RuntimeConstApplyCache
{
	struct ApplyStats
	{
		std::size_t formMapEntries{ 0 };
		std::size_t editorIDFormEntries{ 0 };
		std::size_t gameSettingEntries{ 0 };
		std::size_t applied{ 0 };
		std::size_t skippedNotAllowed{ 0 };
		std::size_t skippedMissingForm{ 0 };
		std::size_t skippedInvalidTarget{ 0 };
		std::size_t skippedUnsupportedType{ 0 };
		std::size_t skippedEmptyText{ 0 };
		std::size_t skippedMissingTarget{ 0 };
		bool mapsBuilt{ false };
		bool mapsReused{ false };
	};

	ApplyStats BuildAndApply(
		const TranslationCatalogBuildResult& catalog,
		const RuntimeApplySettings::Values& settings,
		std::string_view reason);

	ApplyStats Reapply(
		const TranslationCatalogBuildResult& catalog,
		const RuntimeApplySettings::Values& settings,
		std::string_view reason);
}
