// AI CONTEXT: Source-free preload pipeline for XML files and mini plugin indexes.
// Depends on TranslationPipeline declarations plus XML parser/mapping modules.
// Runtime assumptions: version-neutral Fallout 4 data preparation before applying translations.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: uses plugin names, raw form IDs, string IDs, and runtime indexes only.
#include "TranslationPipeline.h"

#include "TranslationPipelineCache.h"
#include "TranslationPreparedData.h"
#include "XmlTranslationMapping.h"
#include "XmlTranslationParser.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <future>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace
{
	struct DedupKey
	{
		std::string addon;
		std::uint32_t formID;
		std::string record;

		bool operator==(const DedupKey& other) const
		{
			return addon == other.addon && formID == other.formID && record == other.record;
		}
	};

	struct DedupKeyHash
	{
		std::size_t operator()(const DedupKey& key) const
		{
			std::size_t h = std::hash<std::string>{}(key.addon);
			h ^= std::hash<std::uint32_t>{}(key.formID) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= std::hash<std::string>{}(key.record) + 0x9e3779b9 + (h << 6) + (h >> 2);
			return h;
		}
	};

	std::optional<std::uint32_t> tryParseBracketFormID(std::string_view edid)
	{
		// [XXXXXXXX] → formID, 用于去重
		return XmlTranslationMapping::ParseBracketFormID(edid);
	}
} // namespace

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

	TranslationPipelineResult Build(const TranslationPipelineOptions& options)
	{
		const PipelinePhase totalPhase{ options, "build total" };
		TranslationPipelineResult result;
		std::vector<std::filesystem::path> xmlFiles;
		{
			const PipelinePhase phase{ options, "discover XML files" };
			xmlFiles = DiscoverXmlFiles(options.xmlDirectory);
		}
		result.discoveredXmlFiles = xmlFiles.size();
		result.cachePath = TranslationPipelineCache::CachePath(options);

		{
			const PipelinePhase phase{ options, "load runtime cache" };
			if (auto cached = TranslationPipelineCache::Load(options, xmlFiles))
			{
				return std::move(*cached);
			}
		}

		std::vector<ParsedXml> parsedFiles;
		std::unordered_map<std::string, std::unordered_set<std::uint32_t>> wantedByPlugin;

		// 并行解析所有 XML 文件
		std::vector<std::future<XmlParseResult>> xmlFutures;
		xmlFutures.reserve(xmlFiles.size());
		for (const auto& xmlPath : xmlFiles)
		{
			xmlFutures.push_back(std::async(std::launch::async, [&xmlPath] {
				return XmlTranslationParser::ParseFile(xmlPath);
			}));
		}

		for (std::size_t i = 0; i < xmlFiles.size(); ++i)
		{
			const PipelinePhase phase{ options, std::string{ "resolve XML " } + xmlFiles[i].filename().string() };
			auto parsed = xmlFutures[i].get();
			if (!parsed.success)
			{
				result.errors.push_back({ xmlFiles[i], parsed.error });
				continue;
			}

			auto plugin = resolvePlugin(parsed.file.addon, options);
			if (!plugin)
			{
				++result.skippedMissingPlugin;
				result.errors.push_back({ xmlFiles[i], "XML Addon is not an active or discoverable plugin" });
				continue;
			}

			parsed.file.addon = plugin->name;
			collectWantedSignatures(parsed.file, wantedByPlugin);
			parsedFiles.push_back({ std::move(parsed.file), std::move(*plugin) });
			++result.parsedXmlFiles;
		}

		// 去重：跨文件合并相同 (Addon, FormID, Record) 的条目，后者覆盖前者
		{
			const PipelinePhase phase{ options, "deduplicate entries across XML files" };
			std::unordered_map<DedupKey, std::size_t, DedupKeyHash> dedupMap;
			dedupMap.reserve(result.parsedXmlFiles * 100);
			std::size_t totalBefore = 0;
			std::size_t totalAfter = 0;

			for (auto& parsed : parsedFiles)
			{
				totalBefore += parsed.file.entries.size();
				std::vector<std::size_t> keep;
				keep.reserve(parsed.file.entries.size());
				for (std::size_t i = 0; i < parsed.file.entries.size(); ++i)
				{
					const auto& entry = parsed.file.entries[i];
					auto formID = tryParseBracketFormID(entry.edid);
					if (!formID)
					{
						keep.push_back(i);
						continue;
					}

					DedupKey key{ parsed.plugin.name, *formID, entry.record };
					auto [it, inserted] = dedupMap.insert({ std::move(key), i });
					if (inserted)
					{
						keep.push_back(i);
					}
				}

				if (keep.size() < parsed.file.entries.size())
				{
					std::vector<XmlTranslationEntry> filtered;
					filtered.reserve(keep.size());
					for (auto i : keep)
					{
						filtered.push_back(std::move(parsed.file.entries[i]));
					}
					parsed.file.entries = std::move(filtered);
				}
				totalAfter += parsed.file.entries.size();
			}

			result.dedupSkippedEntries = totalBefore - totalAfter;
		}

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
			result.savedCache = TranslationPipelineCache::Save(options, xmlFiles, result);
		}
		return result;
	}
}

