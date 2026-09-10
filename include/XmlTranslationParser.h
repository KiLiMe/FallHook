// AI CONTEXT: xTranslator XML parser declarations.
// Depends only on standard containers; implementation uses XmlLite on Windows.
// Runtime assumptions: version-neutral Fallout 4 record data but no game APIs.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: Source text is parsed only as inert metadata and must not affect catalog output.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct XmlTranslationEntry
{
	int list{ -1 };
	std::optional<std::uint32_t> stringID;
	std::optional<std::uint32_t> partial;
	std::optional<std::uint32_t> index;
	std::optional<std::uint32_t> indexMax;
	std::optional<std::uint32_t> formID;
	std::size_t ordinal{ 0 };
	std::size_t recordOrdinal{ 0 };
	std::string edid;
	std::string record;
	std::string source;
	std::string dest;
};

struct XmlTranslationFile
{
	std::filesystem::path path;
	std::string addon;
	std::vector<XmlTranslationEntry> entries;
};

struct XmlParseResult
{
	bool success{ false };
	std::string error;
	XmlTranslationFile file;
};

namespace XmlTranslationParser
{
	XmlParseResult ParseFile(const std::filesystem::path& path);
	XmlParseResult ParseFile(const std::filesystem::path& path, std::string_view recordFilter);
}
