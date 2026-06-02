// AI CONTEXT: Defines cached per-runtime prepared catalog sections.
// Depends on TranslationCatalog records and shared translation type metadata.
// Runtime assumptions: version-neutral Fallout 4 source-free data preparation before hook/runtime map rebuilds.
// Version-specific logic: none; hook offsets remain in runtime modules.
// Source-free policy: filters records by type/signature/identity only; XML Source text is never used.
#pragma once

#include "TranslationCatalog.h"

struct TranslationPreparedData
{
	TranslationCatalogBuildResult constApply;
	TranslationCatalogBuildResult questJournal;
	TranslationCatalogBuildResult pipboyLog;
	TranslationCatalogBuildResult description;
	TranslationCatalogBuildResult dialogue;
	TranslationCatalogBuildResult activationText;
	TranslationCatalogBuildResult perkActivateChoice;
	TranslationCatalogBuildResult fullNameLoad;
	TranslationCatalogBuildResult inventoryTemplateNames;
	TranslationCatalogBuildResult instanceNaming;
};

namespace TranslationPreparedDataBuilder
{
	[[nodiscard]] TranslationPreparedData Build(const TranslationCatalogBuildResult& catalog);
}
