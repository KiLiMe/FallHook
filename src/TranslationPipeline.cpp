// AI CONTEXT: Source-free preload pipeline for XML files and mini plugin indexes.
// Depends on TranslationPipeline declarations plus XML parser/mapping modules.
// Runtime assumptions: version-neutral Fallout 4 data preparation before applying translations.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: uses plugin names, raw form IDs, string IDs, and runtime indexes only.
#include "TranslationPipeline.h"

#include "RuntimeStringOverlay.h"
#include "TranslationPipelineCache.h"
#include "TranslationPreparedData.h"
#include "XmlLoadOrder.h"
#include "XmlTranslationMapping.h"
#include "XmlTranslationParser.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace
{
	std::string lower(std::string_view value)
	{
		std::string result{ value };
		std::ranges::transform(result, result.begin(), [](unsigned char ch) {
			return static_cast<char>(std::tolower(ch));
		});
		return result;
	}

	std::string trim(std::string_view value)
	{
		const auto begin = value.find_first_not_of(" \t\r\n");
		if (begin == std::string_view::npos)
		{
			return {};
		}

		const auto end = value.find_last_not_of(" \t\r\n");
		return std::string{ value.substr(begin, end - begin + 1) };
	}

	bool iequals(std::string_view lhs, std::string_view rhs)
	{
		return lower(lhs) == lower(rhs);
	}

	bool hasPluginExtension(const std::filesystem::path& path)
	{
		const auto ext = lower(path.extension().string());
		return ext == ".esm" || ext == ".esp" || ext == ".esl";
	}

	std::optional<TranslationPipelinePlugin> resolvePlugin(
		std::string_view addon,
		const TranslationPipelineOptions& options)
	{
		const auto wanted = trim(addon);
		if (wanted.empty())
		{
			return std::nullopt;
		}

		for (const auto& plugin : options.plugins)
		{
			if (iequals(plugin.name, wanted))
			{
				return plugin;
			}
		}

		if (!hasPluginExtension(wanted))
		{
			std::optional<TranslationPipelinePlugin> stemMatch;
			for (const auto& plugin : options.plugins)
			{
				if (!iequals(std::filesystem::path{ plugin.name }.stem().string(), wanted))
				{
					continue;
				}
				if (stemMatch)
				{
					return std::nullopt;
				}
				stemMatch = plugin;
			}
			if (stemMatch)
			{
				return stemMatch;
			}
		}

		const auto directPath = options.dataDirectory / std::filesystem::path{ wanted };
		if (std::filesystem::exists(directPath))
		{
			return TranslationPipelinePlugin{ wanted, directPath, 0 };
		}

		if (!hasPluginExtension(wanted))
		{
			std::optional<TranslationPipelinePlugin> suffixMatch;
			for (const auto suffix : { ".esm", ".esp", ".esl" })
			{
				auto candidate = wanted + suffix;
				auto path = options.dataDirectory / std::filesystem::path{ candidate };
				if (!std::filesystem::exists(path))
				{
					continue;
				}
				if (suffixMatch)
				{
					return std::nullopt;
				}
				suffixMatch = TranslationPipelinePlugin{ std::move(candidate), path, 0 };
			}
			return suffixMatch;
		}

		return std::nullopt;
	}

	void collectWantedSignatures(
		const XmlTranslationFile& file,
		std::unordered_map<std::string, std::unordered_set<std::uint32_t>>& wantedByPlugin)
	{
		auto& wanted = wantedByPlugin[lower(file.addon)];
		for (const auto& entry : file.entries)
		{
			if (entry.edid.empty() && !entry.stringID)
			{
				continue;
			}

			const auto signature = XmlTranslationMapping::NormalizeSignature(entry.record);
			if (signature.empty() || signature.starts_with("GMST "))
			{
				continue;
			}

			if (XmlTranslationMapping::GetTranslationType(signature) == TranslationType::kUnknown)
			{
				continue;
			}

			wanted.emplace(PluginEdidIndex::SignatureKey(signature.substr(0, 4)));
		}
	}

	struct ParsedXml
	{
		XmlTranslationFile file;
		TranslationPipelinePlugin plugin;
	};

	class PipelinePhase
	{
	public:
		PipelinePhase(const TranslationPipelineOptions& options, std::string phase) :
			m_options(std::addressof(options)),
			m_phase(std::move(phase))
		{
			report(TranslationPipelinePhaseEvent::kBegin);
		}

		~PipelinePhase()
		{
			report(TranslationPipelinePhaseEvent::kEnd);
		}

		PipelinePhase(const PipelinePhase&) = delete;
		PipelinePhase(PipelinePhase&&) = delete;
		PipelinePhase& operator=(const PipelinePhase&) = delete;
		PipelinePhase& operator=(PipelinePhase&&) = delete;

	private:
		void report(TranslationPipelinePhaseEvent event) const
		{
			if (!m_options || !m_options->progress)
			{
				return;
			}

			try
			{
				m_options->progress(m_phase, event);
			}
			catch (...)
			{}
		}

		const TranslationPipelineOptions* m_options;
		std::string m_phase;
	};
}

namespace TranslationPipeline
{
	std::vector<std::filesystem::path> DiscoverXmlFiles(const std::filesystem::path& directory)
	{
		std::vector<std::filesystem::path> files;
		if (!std::filesystem::exists(directory))
		{
			return files;
		}

		for (const auto& entry : std::filesystem::directory_iterator(directory))
		{
			if (entry.is_regular_file() && lower(entry.path().extension().string()) == ".xml")
			{
				files.push_back(entry.path());
			}
		}

		std::ranges::sort(files);
		return files;
	}

	// Overlay XML files are stored in a separate directory and use only sID-based identity.
	// They are not bound to any ESP plugin; the <Addon> field is ignored.
	//
	// Overlay files are ordered by an explicit layer priority (numeric filename
	// prefix, see XmlLoadOrder::LayerPriority) so that later files overwrite
	// earlier ones deterministically, instead of depending on the order the
	// directory iterator happens to yield.
	std::vector<std::filesystem::path> DiscoverOverlayXmlFiles(const std::filesystem::path& directory)
	{
		auto files = DiscoverXmlFiles(directory);
		std::ranges::stable_sort(files, [](const auto& left, const auto& right) {
			const auto leftPriority = XmlLoadOrder::LayerPriority(left.filename().string());
			const auto rightPriority = XmlLoadOrder::LayerPriority(right.filename().string());
			if (leftPriority != rightPriority)
			{
				return leftPriority < rightPriority;
			}
			return left.filename().string() < right.filename().string();
		});
		return files;
	}

	TranslationPipelineResult Build(const TranslationPipelineOptions& options)
	{
		const PipelinePhase totalPhase{ options, "build total" };
		TranslationPipelineResult result;
		std::vector<std::filesystem::path> xmlFiles;
		{
			const PipelinePhase phase{ options, "discover XML files" };
			xmlFiles = DiscoverXmlFiles(options.xmlDirectory);
		}

		// Also discover overlay XML files from the overlay directory
		std::vector<std::filesystem::path> overlayXmlFiles;
		{
			const PipelinePhase phase{ options, "discover overlay XML files" };
			overlayXmlFiles = DiscoverOverlayXmlFiles(options.overlayDirectory);
		}
		result.discoveredXmlFiles = xmlFiles.size() + overlayXmlFiles.size();
		result.cachePath = TranslationPipelineCache::CachePath(options);

		{
			const PipelinePhase phase{ options, "load runtime cache" };
			std::vector<std::filesystem::path> allXmlFiles = xmlFiles;
			allXmlFiles.insert(allXmlFiles.end(), overlayXmlFiles.begin(), overlayXmlFiles.end());
			if (auto cached = TranslationPipelineCache::Load(options, allXmlFiles))
			{
				return std::move(*cached);
			}
		}

		std::vector<ParsedXml> parsedFiles;
		std::unordered_map<std::string, std::unordered_set<std::uint32_t>> wantedByPlugin;
		for (const auto& xmlPath : xmlFiles)
		{
			const PipelinePhase phase{ options, std::string{ "parse XML " } + xmlPath.filename().string() };
			auto parsed = XmlTranslationParser::ParseFile(xmlPath);
			if (!parsed.success)
			{
				result.errors.push_back({ xmlPath, parsed.error });
				continue;
			}

			auto plugin = resolvePlugin(parsed.file.addon, options);
			if (!plugin)
			{
				++result.skippedMissingPlugin;
				result.errors.push_back({ xmlPath, "XML Addon is not an active or discoverable plugin" });
				continue;
			}

			parsed.file.addon = plugin->name;
			collectWantedSignatures(parsed.file, wantedByPlugin);
			parsedFiles.push_back({ std::move(parsed.file), std::move(*plugin) });
			++result.parsedXmlFiles;
		}

		// Process overlay XML files: sID-only, no plugin binding required
		RuntimeStringOverlay::OverlayMap overlayMap;
		for (const auto& xmlPath : overlayXmlFiles)
		{
			const PipelinePhase phase{ options, std::string{ "parse overlay XML " } + xmlPath.filename().string() };
			auto parsed = XmlTranslationParser::ParseFile(xmlPath);
			if (!parsed.success)
			{
				result.errors.push_back({ xmlPath, parsed.error });
				continue;
			}

			for (const auto& entry : parsed.file.entries)
			{
				if (!entry.stringID || entry.dest.empty())
				{
					continue;
				}
				overlayMap[*entry.stringID] = entry.dest;
			}
			++result.parsedXmlFiles;
		}
		if (!overlayMap.empty())
		{
			RuntimeStringOverlay::SetMap(std::move(overlayMap));
		}
		result.overlayEntries = RuntimeStringOverlay::Count();

		std::unordered_map<std::string, PluginEdidIndex> pluginIndexes;
		for (const auto& parsed : parsedFiles)
		{
			const auto key = lower(parsed.plugin.name);
			if (pluginIndexes.contains(key))
			{
				continue;
			}

			const auto wantedIt = wantedByPlugin.find(key);
			const auto wanted = wantedIt != wantedByPlugin.end() ? wantedIt->second : std::unordered_set<std::uint32_t>{};
			const PipelinePhase phase{ options, std::string{ "load plugin index " } + parsed.plugin.name };
			auto index = PluginEdidIndex::Load(parsed.plugin.path, wanted);
			if (index.loaded())
			{
				++result.loadedPluginIndexes;
			}
			else
			{
				++result.failedPluginIndexes;
			}
			pluginIndexes.emplace(key, std::move(index));
		}

		std::vector<TranslationCatalogFile> catalogFiles;
		catalogFiles.reserve(parsedFiles.size());
		{
			const PipelinePhase phase{ options, "prepare catalog inputs" };
			for (const auto& parsed : parsedFiles)
			{
				const auto key = lower(parsed.plugin.name);
				const auto indexIt = pluginIndexes.find(key);
				const auto* index = indexIt != pluginIndexes.end() && indexIt->second.loaded() ? &indexIt->second : nullptr;
				catalogFiles.push_back(TranslationCatalogFile{ parsed.file, parsed.plugin.priority, index });
			}
		}

		{
			const PipelinePhase phase{ options, "build translation catalog" };
			result.catalog = TranslationCatalog::Build(catalogFiles, options.loadOrderMode);
			result.prepared = TranslationPreparedDataBuilder::Build(result.catalog);
		}
		{
			const PipelinePhase phase{ options, "save runtime cache" };
			std::vector<std::filesystem::path> allXmlFiles = xmlFiles;
			allXmlFiles.insert(allXmlFiles.end(), overlayXmlFiles.begin(), overlayXmlFiles.end());
			result.savedCache = TranslationPipelineCache::Save(options, allXmlFiles, result);
		}
		return result;
	}
}

