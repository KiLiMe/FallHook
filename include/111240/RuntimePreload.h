// AI CONTEXT: Runtime bridge for building the source-free catalog after game data loads.
// Depends on CommonLibF4/F4SE through the plugin target and TranslationPipeline core APIs.
// Runtime scope is Fallout 4 1.11.240 only.
// Version-specific logic: Fallout 4 1.11.240 only; no alternate runtime branches.
// Source-free policy: this builds catalog/index data only and does not apply text or install hooks.
#pragma once

#include "TranslationPipeline.h"

namespace Runtime111240::RuntimePreload
{
	void BuildCatalogOnce();
	[[nodiscard]] const TranslationPipelineResult* GetCatalogBuildResult() noexcept;
}
