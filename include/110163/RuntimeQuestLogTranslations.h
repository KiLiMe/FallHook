// AI CONTEXT: Builds and queries source-free QUST:CNAM translations for the journal text hook.
// Depends on TranslationCatalog data and live form resolution from the plugin runtime.
// Runtime scope is Fallout 4 1.10.163 quest journal stage/item identity maps only.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: lookup keys are quest FormID plus stage item index; never Source text.
#pragma once

#include "TranslationCatalog.h"

#include <cstdint>

namespace RuntimeQuestLogTranslations
{
	struct LookupResult
	{
		const char* text{ nullptr };
		std::uint32_t index{ 0xFFFFFFFFu };
	};

	void Rebuild(const TranslationCatalogBuildResult& catalog);

	[[nodiscard]] LookupResult LookupByStageItem(
		std::uint32_t runtimeQuestFormID,
		std::uint16_t stageIndex,
		std::uint8_t itemIndex);
}
