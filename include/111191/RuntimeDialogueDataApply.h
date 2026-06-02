// AI CONTEXT: Applies source-free dialogue topic records into loaded TESTopic data.
// Depends on TranslationCatalog and CommonLibF4 dialogue forms.
// Runtime scope is Fallout 4 1.11.191 dialogue data mutation during catalog apply only.
// Version-specific logic: applies 1.11.191 DIAL:FULL records.
// Source-free policy: selects targets by DIAL identity; INFO:NAM1 is deferred to response construction.
#pragma once

#include "TranslationCatalog.h"

namespace Runtime111191::RuntimeDialogueDataApply
{
	struct ApplyStats
	{
		std::size_t topicsApplied{ 0 };
		std::size_t responsesDeferred{ 0 };
		std::size_t skippedEmptyText{ 0 };
		std::size_t skippedMissingForm{ 0 };
	};

	ApplyStats Apply(const TranslationCatalogBuildResult& catalog);
}
