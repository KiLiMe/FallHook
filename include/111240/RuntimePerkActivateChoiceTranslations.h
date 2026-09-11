// AI CONTEXT: Stores source-free PERK:EPF2 activate-choice button translations.
// Depends on TranslationCatalog editorID target map only.
// Runtime scope is Fallout 4 1.11.240 perk entry-point activate-choice data for HUDRollover.
// Version-specific logic: none; HUDRollover owns runtime hook addresses.
// Source-free policy: maps by editorID target map plus RoboticsExpert special text; never by Source or live button text.
#pragma once

#include "TranslationCatalog.h"

#include <optional>
#include <string>
#include <string_view>

namespace Runtime111240::RuntimePerkActivateChoiceTranslations
{
	struct RebuildStats
	{
		std::size_t accepted{ 0 };
		std::size_t editorTargetEntries{ 0 };
		std::size_t ambiguousEditorTargets{ 0 };
		std::size_t skippedWrongType{ 0 };
		std::size_t skippedEmptyText{ 0 };
		std::size_t skippedMissingTarget{ 0 };
		std::size_t roboticsExpertHackEntries{ 0 };
	};

	RebuildStats Rebuild(const TranslationCatalogBuildResult& catalog, bool force = false);
	[[nodiscard]] std::optional<std::string> LookupHudSecondaryTarget(std::string_view editorID);
	[[nodiscard]] std::optional<std::string> RoboticsExpertHackText();
}
