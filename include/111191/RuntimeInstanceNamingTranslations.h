// AI CONTEXT: Builds and applies source-free INNR:WNAM rule translations directly to live naming data.
// Depends on TranslationCatalog data and CommonLibF4 BGSInstanceNamingRules runtime forms.
// Runtime scope is Fallout 4 1.11.191 BGSInstanceNamingRules rule text only.
// Version-specific logic: Fallout 4 1.11.191 only; no alternate runtime branches.
// Source-free policy: keys rules by resolved INNR form ID and plugin sID rule slot; never by Source text.
#pragma once

#include "TranslationCatalog.h"

namespace Runtime111191::RuntimeInstanceNamingTranslations
{
	struct RebuildStats
	{
		std::size_t accepted{ 0 };
		std::size_t skippedWrongType{ 0 };
		std::size_t skippedEmptyText{ 0 };
		std::size_t skippedMissingIndex{ 0 };
		std::size_t skippedMissingForm{ 0 };
		std::size_t skippedWrongFormType{ 0 };
		std::size_t appliedRules{ 0 };
		std::size_t unchangedRules{ 0 };
		std::size_t skippedRuntimeForm{ 0 };
		std::size_t skippedSlotOutOfRange{ 0 };
		std::size_t sidRemappedRules{ 0 };
		std::size_t skippedStringIDMismatch{ 0 };
	};

	RebuildStats Rebuild(const TranslationCatalogBuildResult& catalog);
	RebuildStats Reapply(const TranslationCatalogBuildResult& catalog);
}
