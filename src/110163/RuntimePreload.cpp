// AI CONTEXT: Builds FallHook's source-free XML/plugin catalog from live Fallout 4 load order.
// Depends on PCH/CommonLibF4 for TESDataHandler and on TranslationPipeline for core loading.
// Runtime scope is Fallout 4 1.10.163 only.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: no source-text fallback maps and no in-game text mutation happen here.
#include "PCH.h"

#include "RuntimeLoadWatchdog.h"
#include "110163/RuntimePreload.h"

#include <Windows.h>

#ifdef ERROR
#	undef ERROR
#endif

#include <chrono>
#include <mutex>

namespace
{
	std::once_flag g_buildCatalogOnce;
	std::optional<TranslationPipelineResult> g_catalogResult;

	[[nodiscard]] double elapsedMs(std::chrono::steady_clock::time_point started)
	{
		return static_cast<double>(
			std::chrono::duration_cast<std::chrono::microseconds>(
				std::chrono::steady_clock::now() - started)
				.count()) /
			1000.0;
	}

	std::filesystem::path gameDirectory()
	{
		std::wstring buffer(MAX_PATH, L'\0');
		const auto size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
		if (size == 0)
		{
			return {};
		}

		buffer.resize(size);
		return std::filesystem::path{ buffer }.parent_path();
	}

	std::vector<TranslationPipelinePlugin> activePlugins(const std::filesystem::path& dataDirectory)
	{
		std::vector<TranslationPipelinePlugin> plugins;
		const auto handler = RE::TESDataHandler::GetSingleton();
		if (!handler)
		{
			return plugins;
		}

		std::uint32_t priority = 0;
		const auto addFile = [&](const RE::TESFile* file) {
			if (!file || !file->IsActive() || file->GetFilename().empty())
			{
				return;
			}

			const auto name = std::string{ file->GetFilename() };
			plugins.push_back(TranslationPipelinePlugin{
				.name = name,
				.path = dataDirectory / std::filesystem::path{ name },
				.priority = priority++
			});
		};

		for (const auto* file : handler->compiledFileCollection.files)
		{
			addFile(file);
		}
		for (const auto* file : handler->compiledFileCollection.smallFiles)
		{
			addFile(file);
		}

		return plugins;
	}

	void pipelineProgress(std::string_view phase, TranslationPipelinePhaseEvent event)
	{
		if (!RuntimeLoadWatchdog::Enabled())
		{
			return;
		}

		std::string label{ "TranslationPipeline " };
		label.append(phase);
		if (event == TranslationPipelinePhaseEvent::kBegin)
		{
			RuntimeLoadWatchdog::SetWaitPhase(label);
		}
		else
		{
			RuntimeLoadWatchdog::ClearWaitPhase(label);
		}
	}
}

namespace RuntimePreload
{
	void BuildCatalogOnce()
	{
		std::call_once(g_buildCatalogOnce, []() {
			RuntimeLoadWatchdog::ScopedPhase totalPhase{ "RuntimePreload BuildCatalogOnce total", 0.0 };

			std::filesystem::path root;
			std::filesystem::path dataDirectory;
			std::filesystem::path xmlDirectory;
			{
				RuntimeLoadWatchdog::ScopedPhase phase{ "RuntimePreload resolve directories", 0.0 };
				root = gameDirectory();
				dataDirectory = root / "Data";
				xmlDirectory = dataDirectory / "F4SE" / "Plugins" / "FallHook";
			}

			TranslationPipelineOptions options;
			options.xmlDirectory = xmlDirectory;

			options.dataDirectory = dataDirectory;
			options.progress = pipelineProgress;
			options.loadOrderMode = XmlLoadOrder::Mode::kPlugin;
			options.runtimePreparedOnly = true;
			{
				RuntimeLoadWatchdog::ScopedPhase phase{ "RuntimePreload enumerate active plugins", 0.0 };
				options.plugins = activePlugins(dataDirectory);
			}

			REX::INFO(
				"{} building source-free catalog from {} with {} active plugin(s).",
				Plugin::NAME,
				xmlDirectory.string(),
				options.plugins.size());

			TranslationPipelineResult result;
			{
				RuntimeLoadWatchdog::ScopedPhase phase{ "RuntimePreload TranslationPipeline::Build", 0.0 };
				result = TranslationPipeline::Build(options);
			}
			REX::INFO(
				"{} source-free catalog built: cacheHit={} cacheSaved={} runtimePreparedOnly={} cache={} discoveredXml={} parsedXml={} missingPlugin={} pluginIndexesLoaded={} pluginIndexesFailed={} records={} accepted={} overwritten={} sidIndexes={} skippedUnknown={} skippedNoIdentity={} skippedEmptyDest={} errors={}.",
				Plugin::NAME,
				result.loadedFromCache,
				result.savedCache,
				options.runtimePreparedOnly,
				result.cachePath.string(),
				result.discoveredXmlFiles,
				result.parsedXmlFiles,
				result.skippedMissingPlugin,
				result.loadedPluginIndexes,
				result.failedPluginIndexes,
				result.catalog.records.empty() ? result.catalog.acceptedEntries : result.catalog.records.size(),
				result.catalog.acceptedEntries,
				result.catalog.overwrittenEntries,
				result.catalog.pluginStringIDIndexes,
				result.catalog.skippedUnknownType,
				result.catalog.skippedWithoutIdentity,
				result.catalog.skippedEmptyDest,
				result.errors.size());

			for (std::size_t i = 0; i < result.errors.size() && i < 16; ++i)
			{
				REX::WARN(
					"{} source-free catalog input issue file={} message={}",
					Plugin::NAME,
					result.errors[i].path.string(),
					result.errors[i].message);
			}

			const auto storeStarted = std::chrono::steady_clock::now();
			g_catalogResult = std::move(result);
			REX::INFO("{} runtime preload store complete: elapsedMs={:.2f}.", Plugin::NAME, elapsedMs(storeStarted));
		});
	}

	const TranslationPipelineResult* GetCatalogBuildResult() noexcept
	{
		return g_catalogResult ? std::addressof(*g_catalogResult) : nullptr;
	}
}
