// AI CONTEXT: Provides bounded safe string copying for InGameText runtime hooks.
// Depends on RuntimeMemorySafety in the .cpp and standard UTF-8/wide string storage.
// Runtime scope is Fallout 4 1.10.163 UI hook pointer reads only.
// Version-specific logic: none.
// Source-free policy: copies text for logging/translation paths; never decides identity.
#pragma once

#include <cstddef>
#include <string>

namespace RuntimeInGameTextSafeCopy
{
	[[nodiscard]] std::wstring Wide(const wchar_t* text, std::size_t limit = 2048);
}
