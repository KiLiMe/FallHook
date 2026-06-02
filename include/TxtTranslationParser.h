// AI CONTEXT: TXT translation dictionary parser declarations.
// Depends only on standard filesystem and strings.
// Runtime scope is independent of game version.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: TXT dictionaries are parsed as payload maps, not primary identity for records.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct TxtTranslationEntry
{
	std::size_t lineNumber{ 0 };
	std::string source;
	std::string dest;
};

struct TxtTranslationFile
{
	std::filesystem::path path;
	std::vector<TxtTranslationEntry> entries;
};

struct TxtParseResult
{
	bool success{ false };
	std::string error;
	TxtTranslationFile file;
};

namespace TxtTranslationParser
{
	TxtParseResult ParseFile(const std::filesystem::path& path);
}
