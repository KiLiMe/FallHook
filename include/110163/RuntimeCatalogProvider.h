// AI CONTEXT: Provides shared lazy access to the source-free runtime catalog for hooks.
// Depends on RuntimePreload and live TESDataHandler plugin-list readiness.
// Runtime scope is Fallout 4 1.10.163 catalog availability for runtime hook maps.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: returns catalog identity/destination data only; no Source lookup exists here.
#pragma once

#include "TranslationPipeline.h"

#include <string_view>

namespace RuntimeCatalogProvider
{
	[[nodiscard]] bool HasLoadedPluginList() noexcept;
	[[nodiscard]] const TranslationPipelineResult* Ensure(std::string_view phaseName);
}
