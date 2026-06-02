// AI CONTEXT: Emits raw HIT/MISS logs for the approved in-game TXT source lookup hook.
// Depends on runtime text conversion through RuntimeInGameTextDictionary.
// Runtime assumptions: version-neutral UI text diagnostics used by selected modules.
// Version-specific logic: none; concrete hook modules provide offsets.
// Source-free policy: logs only the explicit [InGameTextHook] TXT source-key exception.
#pragma once

#include <string>
#include <string_view>

namespace RuntimeInGameTextLog
{
	void Configure(bool logRaw) noexcept;
	[[nodiscard]] bool Enabled() noexcept;
	[[nodiscard]] std::string FormatAddress(const void* address);
	[[nodiscard]] std::string FormatCallStack(std::uint32_t skipFrames, std::uint32_t maxFrames);
	void Hit(std::wstring_view raw, std::wstring_view text, std::wstring_view dest);
	void Miss(std::wstring_view raw, std::wstring_view text);
}
