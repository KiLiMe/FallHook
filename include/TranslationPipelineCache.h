// AI CONTEXT: Binary runtime cache for source-free translation pipeline output.
// Depends on TranslationPipeline result/options types and filesystem stamps.
// Runtime assumptions: version-neutral Fallout 4 XML/plugin catalog reuse.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: cache stores catalog identity/destination data only, never original Source text.
#pragma once

#include "TranslationPipeline.h"

#include <filesystem>
#include <optional>
#include <span>

namespace TranslationPipelineCache
{
	[[nodiscard]] std::filesystem::path CachePath(const TranslationPipelineOptions& options);
	[[nodiscard]] std::optional<TranslationPipelineResult> Load(
		const TranslationPipelineOptions& options,
		std::span<const std::filesystem::path> xmlFiles);
	[[nodiscard]] bool Save(
		const TranslationPipelineOptions& options,
		std::span<const std::filesystem::path> xmlFiles,
		const TranslationPipelineResult& result);
}
