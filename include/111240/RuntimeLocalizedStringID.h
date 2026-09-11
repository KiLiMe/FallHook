// AI CONTEXT: Reads source-free sID values preserved in BGSLocalizedString storage.
// Depends on CommonLibF4 BGSLocalizedString declarations only.
// Runtime scope is Fallout 4 1.11.240 localized string fields that keep <ID=XXXXXXXX> prefixes.
// Version-specific logic: none; this is prefix parsing only.
// Source-free policy: extracts stable string IDs, never reads or matches original display text.
#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace RE
{
	class BGSLocalizedString;
}

namespace Runtime111240::RuntimeLocalizedStringID
{
	[[nodiscard]] std::optional<std::uint32_t> Parse(std::string_view text);
	[[nodiscard]] std::optional<std::uint32_t> Read(const RE::BGSLocalizedString& text);
}
