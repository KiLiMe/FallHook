// AI CONTEXT: Holds source-free runtime description translations keyed by resolved form identity.
// Depends on TranslationCatalog records and CommonLibF4 TESDescription/TESForm runtime objects.
// Runtime scope is Fallout 4 1.10.163 TESDescription-backed DESC records.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: maps by form ID and description owner pointer, never by original text.
#pragma once

#include "TranslationCatalog.h"

namespace RE
{
	class TESDescription;
	class TESForm;
}

namespace RuntimeDescriptionTranslations
{
	struct LookupResult
	{
		std::uint32_t formID{ 0 };
		std::string text;
		bool usedOwnerIndex{ false };
	};

	void Rebuild(const TranslationCatalogBuildResult& catalog);
	[[nodiscard]] std::optional<LookupResult> Lookup(const RE::TESDescription* description, const RE::TESForm* form);
}
