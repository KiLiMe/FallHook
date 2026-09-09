// AI CONTEXT: Pre-translation XML/plugin-index pipeline for source-free catalog building.
// Depends on PluginEdidIndex, TranslationCatalog, and XML parsing/load-order modules.
// Runtime assumptions: version-neutral Fallout 4 plugin data before any in-game text mutation.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: builds form/string-slot identities and never creates source-text fallback maps.
#pragma once

#include "PluginEdidIndex.h"
#include "TranslationCatalog.h"
#include "TranslationPreparedData.h"
#include "XmlLoadOrder.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

struct TranslationPipelinePlugin
{
	std::string name;
	std::filesystem::path path;
	std::uint32_t priority{ 0 };
};

enum class TranslationPipelinePhaseEvent
{
	kBegin,
	kEnd
};

struct TranslationPipelineOptions
{
	std::filesystem::path xmlDirectory;
	std::filesystem::path overlayDirectory;
	std::filesystem::path dataDirectory;
	std::vector<TranslationPipelinePlugin> plugins;
	std::function<void(std::string_view, TranslationPipelinePhaseEvent)> progress;
	XmlLoadOrder::Mode loadOrderMode{ XmlLoadOrder::Mode::kPlugin };
	bool runtimePreparedOnly{ false };
};

struct TranslationPipelineError
{
	std::filesystem::path path;
	std::string message;
};

struct TranslationPipelineResult
{
	TranslationCatalogBuildResult catalog;
	TranslationPreparedData prepared;
	std::vector<TranslationPipelineError> errors;
	std::filesystem::path cachePath;
	std::size_t discoveredXmlFiles{ 0 };
	std::size_t parsedXmlFiles{ 0 };
	std::size_t skippedMissingPlugin{ 0 };
	std::size_t loadedPluginIndexes{ 0 };
	std::size_t failedPluginIndexes{ 0 };
	// stringID-only overlay entries loaded from Overlay/ (ESP-independent).
	std::size_t overlayEntries{ 0 };
	bool loadedFromCache{ false };
	bool savedCache{ false };
};

namespace TranslationPipeline
{
	std::vector<std::filesystem::path> DiscoverXmlFiles(const std::filesystem::path& directory);
	TranslationPipelineResult Build(const TranslationPipelineOptions& options);
}
