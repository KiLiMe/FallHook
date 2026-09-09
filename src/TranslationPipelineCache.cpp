// AI CONTEXT: Loads and saves FallHook's source-free translation pipeline cache.
// Depends on TranslationPipelineCache declarations and binary helper routines in this file.
// Runtime assumptions: version-neutral Fallout 4 catalog reuse across boots.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: serialized records exclude XML Source text and source fallback maps.
#include "TranslationPipelineCache.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>

namespace
{
	constexpr std::uint64_t kMagic = 0x3143484350484C46ull;
	constexpr std::uint32_t kVersion = 14;
	constexpr std::uint64_t kMaxRecords = 2'000'000;
	constexpr std::uint32_t kMaxString = 32 * 1024 * 1024;

	struct FileStamp
	{
		std::string path;
		std::uint64_t size{ 0 };
		std::int64_t writeTime{ 0 };
	};

	std::string lower(std::string_view value)
	{
		std::string out{ value };
		std::ranges::transform(out, out.begin(), [](unsigned char ch) {
			return static_cast<char>(std::tolower(ch));
		});
		return out;
	}

	FileStamp stampFile(const std::filesystem::path& path)
	{
		std::error_code ec;
		const auto size = std::filesystem::file_size(path, ec);
		const auto writeTime = std::filesystem::last_write_time(path, ec);
		return FileStamp{
			.path = path.string(),
			.size = ec ? 0 : static_cast<std::uint64_t>(size),
			.writeTime = ec ? 0 : static_cast<std::int64_t>(writeTime.time_since_epoch().count())
		};
	}

	bool sameStamp(const FileStamp& lhs, const FileStamp& rhs)
	{
		return lower(lhs.path) == lower(rhs.path) &&
			lhs.size == rhs.size &&
			lhs.writeTime == rhs.writeTime;
	}

	template <class T>
	bool writePod(std::ostream& output, const T& value)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		output.write(reinterpret_cast<const char*>(std::addressof(value)), sizeof(T));
		return static_cast<bool>(output);
	}

	template <class T>
	bool readPod(std::istream& input, T& value)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		input.read(reinterpret_cast<char*>(std::addressof(value)), sizeof(T));
		return static_cast<bool>(input);
	}

	bool writeString(std::ostream& output, std::string_view value)
	{
		const auto size = static_cast<std::uint32_t>(std::min<std::size_t>(
			value.size(),
			std::numeric_limits<std::uint32_t>::max()));
		return writePod(output, size) &&
			(output.write(value.data(), size), static_cast<bool>(output));
	}

	bool readString(std::istream& input, std::string& value)
	{
		std::uint32_t size = 0;
		if (!readPod(input, size) || size > kMaxString)
		{
			return false;
		}

		value.assign(size, '\0');
		if (size == 0)
		{
			return true;
		}
		input.read(value.data(), size);
		return static_cast<bool>(input);
	}

	bool writeStamp(std::ostream& output, const FileStamp& stamp)
	{
		return writeString(output, stamp.path) &&
			writePod(output, stamp.size) &&
			writePod(output, stamp.writeTime);
	}

	bool readStamp(std::istream& input, FileStamp& stamp)
	{
		return readString(input, stamp.path) &&
			readPod(input, stamp.size) &&
			readPod(input, stamp.writeTime);
	}

	bool writeOptionalU32(std::ostream& output, const std::optional<std::uint32_t>& value)
	{
		const std::uint8_t present = value ? 1 : 0;
		return writePod(output, present) && writePod(output, value.value_or(0));
	}

	bool readOptionalU32(std::istream& input, std::optional<std::uint32_t>& value)
	{
		std::uint8_t present = 0;
		std::uint32_t stored = 0;
		if (!readPod(input, present) || !readPod(input, stored))
		{
			return false;
		}
		value = present ? std::optional<std::uint32_t>{ stored } : std::nullopt;
		return true;
	}

	bool writeOptionalString(std::ostream& output, const std::optional<std::string>& value)
	{
		const std::uint8_t present = value && !value->empty() ? 1 : 0;
		return writePod(output, present) && (present ? writeString(output, *value) : true);
	}

	bool readOptionalString(std::istream& input, std::optional<std::string>& value)
	{
		std::uint8_t present = 0;
		if (!readPod(input, present))
		{
			return false;
		}
		if (!present)
		{
			value = std::nullopt;
			return true;
		}
		std::string text;
		if (!readString(input, text))
		{
			return false;
		}
		value = std::move(text);
		return true;
	}

	bool writeData(std::ostream& output, const SourceFreeTranslationData& data)
	{
		const auto type = static_cast<std::uint8_t>(data.translationType);
		return writePod(output, type) &&
			writeString(output, data.replacerText) &&
			writeOptionalU32(output, data.formID) &&
			writeOptionalU32(output, data.index) &&
			writeOptionalU32(output, data.stringID) &&
			writeOptionalU32(output, data.responseID) &&
			writeOptionalString(output, data.editorID);
	}

	bool readData(std::istream& input, SourceFreeTranslationData& data)
	{
		std::uint8_t type = 0;
		if (!readPod(input, type) || !readString(input, data.replacerText))
		{
			return false;
		}
		data.translationType = static_cast<TranslationType>(type);
		return readOptionalU32(input, data.formID) &&
			readOptionalU32(input, data.index) &&
			readOptionalU32(input, data.stringID) &&
			readOptionalU32(input, data.responseID) &&
			readOptionalString(input, data.editorID);
	}

	bool writeCatalogMetadata(std::ostream& output, const TranslationCatalogBuildResult& catalog)
	{
		if (!writePod(output, catalog.totalEntries) ||
			!writePod(output, catalog.acceptedEntries) ||
			!writePod(output, catalog.overwrittenEntries) ||
			!writePod(output, catalog.skippedUnknownType) ||
			!writePod(output, catalog.skippedWithoutIdentity) ||
			!writePod(output, catalog.skippedEmptyDest) ||
			!writePod(output, catalog.pluginStringIDIndexes))
		{
			return false;
		}
		return true;
	}

	bool readCatalogMetadata(std::istream& input, TranslationCatalogBuildResult& catalog)
	{
		catalog.records.clear();
		return readPod(input, catalog.totalEntries) &&
			readPod(input, catalog.acceptedEntries) &&
			readPod(input, catalog.overwrittenEntries) &&
			readPod(input, catalog.skippedUnknownType) &&
			readPod(input, catalog.skippedWithoutIdentity) &&
			readPod(input, catalog.skippedEmptyDest) &&
			readPod(input, catalog.pluginStringIDIndexes);
	}

	bool writeCatalogRecords(std::ostream& output, const std::vector<TranslationCatalogRecord>& records)
	{
		if (!writePod(output, static_cast<std::uint64_t>(records.size())))
		{
			return false;
		}
		for (const auto& record : records)
		{
			if (!writeString(output, record.key) ||
				!writeData(output, record.data) ||
				!writeString(output, record.pluginName) ||
				!writeString(output, record.recordSignature))
			{
				return false;
			}
		}
		return true;
	}

	bool readCatalogRecords(std::istream& input, std::vector<TranslationCatalogRecord>& records)
	{
		std::uint64_t recordCount = 0;
		if (!readPod(input, recordCount) || recordCount > kMaxRecords)
		{
			return false;
		}
		records.resize(static_cast<std::size_t>(recordCount));
		for (auto& record : records)
		{
			if (!readString(input, record.key) ||
				!readData(input, record.data) ||
				!readString(input, record.pluginName) ||
				!readString(input, record.recordSignature))
			{
				return false;
			}
		}
		return true;
	}

	bool writeCatalog(std::ostream& output, const TranslationCatalogBuildResult& catalog)
	{
		return writeCatalogMetadata(output, catalog) && writeCatalogRecords(output, catalog.records);
	}

	bool readCatalog(std::istream& input, TranslationCatalogBuildResult& catalog)
	{
		return readCatalogMetadata(input, catalog) && readCatalogRecords(input, catalog.records);
	}

	bool writePrepared(std::ostream& output, const TranslationPreparedData& prepared)
	{
		return writeCatalog(output, prepared.constApply) &&
			writeCatalog(output, prepared.questJournal) &&
			writeCatalog(output, prepared.pipboyLog) &&
			writeCatalog(output, prepared.description) &&
			writeCatalog(output, prepared.dialogue) &&
			writeCatalog(output, prepared.activationText) &&
			writeCatalog(output, prepared.perkActivateChoice) &&
			writeCatalog(output, prepared.fullNameLoad) &&
			writeCatalog(output, prepared.inventoryTemplateNames) &&
			writeCatalog(output, prepared.instanceNaming);
	}

	bool readPrepared(std::istream& input, TranslationPreparedData& prepared)
	{
		return readCatalog(input, prepared.constApply) &&
			readCatalog(input, prepared.questJournal) &&
			readCatalog(input, prepared.pipboyLog) &&
			readCatalog(input, prepared.description) &&
			readCatalog(input, prepared.dialogue) &&
			readCatalog(input, prepared.activationText) &&
			readCatalog(input, prepared.perkActivateChoice) &&
			readCatalog(input, prepared.fullNameLoad) &&
			readCatalog(input, prepared.inventoryTemplateNames) &&
			readCatalog(input, prepared.instanceNaming);
	}
}

namespace TranslationPipelineCache
{
	std::filesystem::path CachePath(const TranslationPipelineOptions& options)
	{
		return options.xmlDirectory / "FallHook.runtime.cache";
	}

	std::optional<TranslationPipelineResult> Load(
		const TranslationPipelineOptions& options,
		std::span<const std::filesystem::path> xmlFiles)
	{
		std::ifstream input(CachePath(options), std::ios::binary);
		if (!input || xmlFiles.empty())
		{
			return std::nullopt;
		}

		std::uint64_t magic = 0;
		std::uint32_t version = 0;
		std::uint32_t mode = 0;
		if (!readPod(input, magic) || !readPod(input, version) || !readPod(input, mode) ||
			magic != kMagic || version != kVersion || mode != static_cast<std::uint32_t>(options.loadOrderMode))
		{
			return std::nullopt;
		}

		std::uint32_t count = 0;
		if (!readPod(input, count) || count != xmlFiles.size())
		{
			return std::nullopt;
		}
		for (std::uint32_t i = 0; i < count; ++i)
		{
			FileStamp cached;
			if (!readStamp(input, cached) || !sameStamp(cached, stampFile(xmlFiles[i])))
			{
				return std::nullopt;
			}
		}

		if (!readPod(input, count) || count != options.plugins.size())
		{
			return std::nullopt;
		}
		for (std::uint32_t i = 0; i < count; ++i)
		{
			std::string name;
			std::uint32_t priority = 0;
			FileStamp cached;
			if (!readString(input, name) || !readPod(input, priority) || !readStamp(input, cached))
			{
				return std::nullopt;
			}
			if (lower(name) != lower(options.plugins[i].name) ||
				priority != options.plugins[i].priority ||
				!sameStamp(cached, stampFile(options.plugins[i].path)))
			{
				return std::nullopt;
			}
		}

		TranslationPipelineResult result;
		result.cachePath = CachePath(options);
		result.loadedFromCache = true;
		if (!readPod(input, result.discoveredXmlFiles) ||
			!readPod(input, result.parsedXmlFiles) ||
			!readPod(input, result.skippedMissingPlugin) ||
			!readPod(input, result.loadedPluginIndexes) ||
			!readPod(input, result.failedPluginIndexes) ||
			!readPod(input, result.overlayEntries))
		{
			return std::nullopt;
		}

		if (!readCatalogMetadata(input, result.catalog) || !readPrepared(input, result.prepared))
		{
			return std::nullopt;
		}
		if (!options.runtimePreparedOnly && !readCatalogRecords(input, result.catalog.records))
		{
			return std::nullopt;
		}
		return result;
	}

	bool Save(
		const TranslationPipelineOptions& options,
		std::span<const std::filesystem::path> xmlFiles,
		const TranslationPipelineResult& result)
	{
		if (xmlFiles.empty() || result.catalog.records.empty())
		{
			return false;
		}

		std::filesystem::create_directories(options.xmlDirectory);
		const auto cachePath = CachePath(options);
		const auto tempPath = cachePath.string() + ".tmp";
		std::ofstream output(tempPath, std::ios::binary | std::ios::trunc);
		if (!output)
		{
			return false;
		}

		const auto mode = static_cast<std::uint32_t>(options.loadOrderMode);
		if (!writePod(output, kMagic) || !writePod(output, kVersion) || !writePod(output, mode) ||
			!writePod(output, static_cast<std::uint32_t>(xmlFiles.size())))
		{
			return false;
		}
		for (const auto& xml : xmlFiles)
		{
			if (!writeStamp(output, stampFile(xml)))
			{
				return false;
			}
		}

		if (!writePod(output, static_cast<std::uint32_t>(options.plugins.size())))
		{
			return false;
		}
		for (const auto& plugin : options.plugins)
		{
			if (!writeString(output, plugin.name) ||
				!writePod(output, plugin.priority) ||
				!writeStamp(output, stampFile(plugin.path)))
			{
				return false;
			}
		}

		if (!writePod(output, result.discoveredXmlFiles) ||
			!writePod(output, result.parsedXmlFiles) ||
			!writePod(output, result.skippedMissingPlugin) ||
			!writePod(output, result.loadedPluginIndexes) ||
			!writePod(output, result.failedPluginIndexes) ||
			!writePod(output, result.overlayEntries) ||
			!writeCatalogMetadata(output, result.catalog) ||
			!writePrepared(output, result.prepared) ||
			!writeCatalogRecords(output, result.catalog.records))
		{
			return false;
		}

		output.close();
		if (!output)
		{
			return false;
		}

		std::error_code ec;
		std::filesystem::rename(tempPath, cachePath, ec);
		if (ec)
		{
			std::filesystem::remove(cachePath, ec);
			ec.clear();
			std::filesystem::rename(tempPath, cachePath, ec);
		}
		return !ec;
	}
}
