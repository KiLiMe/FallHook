// AI CONTEXT: Stores source-free PERK:EPF2 activate-choice button translations.
// Depends on TranslationCatalog and CommonLibF4 form identity only.
// Runtime scope is Fallout 4 1.10.163 perk entry-point activate-choice data.
// Version-specific logic: none; HUDRollover owns runtime hook addresses.
// Source-free policy: maps by PERK form/index/sID identity; never by Source or live button text.
#pragma once

#include "TranslationCatalog.h"

#include <optional>
#include <string>
#include <string_view>

namespace RE
{
	class TESForm;
}

namespace RuntimePerkActivateChoiceTranslations
{
	struct RebuildStats
	{
		std::size_t accepted{ 0 };
		std::size_t formIndexEntries{ 0 };
		std::size_t formSidEntries{ 0 };
		std::size_t editorTargetEntries{ 0 };
		std::size_t ambiguousEditorTargets{ 0 };
		std::size_t uniqueSidEntries{ 0 };
		std::size_t ambiguousSidEntries{ 0 };
		std::size_t skippedWrongType{ 0 };
		std::size_t skippedEmptyText{ 0 };
		std::size_t skippedMissingTarget{ 0 };
		std::size_t skippedUnresolvedFormID{ 0 };
		std::size_t roboticsExpertHackEntries{ 0 };
	};

	RebuildStats Rebuild(const TranslationCatalogBuildResult& catalog, bool force = false);
	[[nodiscard]] std::optional<std::string> Lookup(
		const RE::TESForm* perk,
		std::optional<std::uint32_t> index,
		std::optional<std::uint32_t> stringID);
	[[nodiscard]] std::optional<std::string> LookupHudSecondaryTarget(std::string_view editorID);
	[[nodiscard]] std::optional<std::string> DefaultText();
	[[nodiscard]] std::optional<std::string> RoboticsExpertHackText();
}
