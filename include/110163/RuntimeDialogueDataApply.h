// AI CONTEXT: Applies source-free dialogue topic names into loaded TESTopic data.
// Depends on TranslationCatalog and CommonLibF4 dialogue topic forms.
// Runtime scope is Fallout 4 1.10.163 DIAL:FULL catalog apply only.
// Version-specific logic: implementation resolves 1.10.163 runtime form IDs.
// Source-free policy: selects targets by DIAL identity; INFO:RNAM is handled only by dialogue button hooks.
#pragma once

#include "TranslationCatalog.h"

namespace RuntimeDialogueDataApply
{
	struct ApplyStats
	{
		std::size_t topicsApplied{ 0 };
		std::size_t skippedWrongType{ 0 };
		std::size_t skippedEmptyText{ 0 };
		std::size_t skippedMissingForm{ 0 };
	};

	ApplyStats Apply(const TranslationCatalogBuildResult& catalog);
}
