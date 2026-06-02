// AI CONTEXT: UTF-8 normalization helper for translation payloads.
// Depends only on standard strings; implementation uses Windows NormalizeString.
// Runtime scope is independent of game version.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: normalization is for payload comparison/cleanup, not lookup identity.
#pragma once

#include <string>
#include <string_view>

namespace TextNormalization
{
	std::string NormalizeUtf8(std::string_view text);
}
