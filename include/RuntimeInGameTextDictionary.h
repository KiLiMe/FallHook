// AI CONTEXT: Owns the approved source-keyed TXT dictionary for global UI text hooks.
// Depends on TXT parsing and text conversion helpers in the runtime plugin target.
// Runtime assumptions: version-neutral dictionary storage; selected modules own UI hook offsets.
// Version-specific logic: none; no hook sites or runtime layout assumptions live here.
// Source-free policy: this is an explicit [InGameTextHook] TXT exception; XML lookup stays source-free.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace RuntimeInGameTextDictionary
{
	struct LoadStats
	{
		bool enabled{ false };
		std::filesystem::path directory;
		std::size_t files{ 0 };
		std::size_t parsedEntries{ 0 };
		std::size_t storedKeys{ 0 };
		std::size_t errors{ 0 };
	};

	[[nodiscard]] LoadStats LoadFromDisk(bool enabled);
	[[nodiscard]] std::uint64_t Revision() noexcept;
	[[nodiscard]] std::size_t EntryCount() noexcept;
	[[nodiscard]] std::optional<std::wstring> Lookup(std::wstring_view raw);
	[[nodiscard]] std::vector<std::uint8_t> BuildScaleformUtf16Txt(std::size_t& entryCount);
	[[nodiscard]] std::wstring NormalizeKey(std::wstring_view value);
	[[nodiscard]] std::wstring_view TrimText(std::wstring_view value) noexcept;
	[[nodiscard]] bool ShouldCapture(std::wstring_view value) noexcept;
	[[nodiscard]] std::wstring Utf8ToWide(std::string_view input);
	[[nodiscard]] std::string WideToUtf8(std::wstring_view input);
}
