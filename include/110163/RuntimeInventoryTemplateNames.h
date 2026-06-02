// AI CONTEXT: Applies indexed WEAP/ARMO FULL records into loaded inventory template name slots.
// Depends on TranslationCatalog records; runtime object access stays inside the .cpp.
// Runtime scope is Fallout 4 1.10.163 weapon/armor instance-name fragment storage.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: uses sID, form/editor identity, and REC id indexes; never matches Source text.
#pragma once

#include "TranslationCatalog.h"

#include <cstddef>

namespace RuntimeInventoryTemplateNames
{
	struct ApplyStats
	{
		std::size_t applied{ 0 };
		std::size_t skippedWrongType{ 0 };
		std::size_t skippedEmptyText{ 0 };
		std::size_t skippedMissingForm{ 0 };
		std::size_t skippedWrongFormType{ 0 };
		std::size_t skippedMissingTemplateItem{ 0 };
		std::size_t skippedIndexOutOfRange{ 0 };
		std::size_t skippedStringIDMismatch{ 0 };
	};

	ApplyStats Rebuild(const TranslationCatalogBuildResult& catalog);
	ApplyStats Apply(const TranslationCatalogBuildResult& catalog, bool force = false);
}
