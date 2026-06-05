// AI CONTEXT: Holds source-free GMST translations used by the Pip-Boy log hook.
// Depends on TranslationCatalog records produced from XML GMST:DATA entries.
// Runtime scope is Fallout 4 1.10.163 Pip-Boy log stat labels only.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: lookup stores GMST editor IDs and destination text, never Source text.
#pragma once

#include "TranslationCatalog.h"

#include <optional>
#include <string>
#include <string_view>

namespace RuntimePipboyLogTranslations
{
	struct LookupResult
	{
		std::string editorID;
		const char* text{ nullptr };
	};

	void Rebuild(const TranslationCatalogBuildResult& catalog);
	[[nodiscard]] std::optional<LookupResult> LookupStatKey(std::string_view key);
}
