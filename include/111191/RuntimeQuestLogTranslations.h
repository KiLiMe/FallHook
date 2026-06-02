// AI CONTEXT: Builds and queries optimized source-free QUST:CNAM maps for the journal hook.
// Depends on TranslationCatalog data and live form resolution from the plugin runtime.
// Runtime scope is Fallout 4 1.11.191 quest journal string/stage/item identity maps only.
// Version-specific logic: lookup accepts the 1.11.191 TESQuestStageItem identity shape.
// Source-free policy: lookup keys are quest FormID plus string ID or stage item index; never Source text.
#pragma once

#include "TranslationCatalog.h"

#include <cstdint>
#include <optional>

namespace Runtime111191::RuntimeQuestLogTranslations
{
	enum class LookupMode : std::uint8_t
	{
		kNone,
		kStringID,
		kIndex
	};

	struct LookupResult
	{
		const char* text{ nullptr };
		std::uint32_t index{ 0xFFFFFFFFu };
		LookupMode mode{ LookupMode::kNone };
	};

	void Rebuild(const TranslationCatalogBuildResult& catalog);

	[[nodiscard]] LookupResult LookupByIdentity(
		std::uint32_t runtimeQuestFormID,
		std::uint32_t stringID,
		std::optional<std::uint32_t> owningStage,
		std::optional<std::uint32_t> itemIndex,
		std::uint32_t currentStage);
}
