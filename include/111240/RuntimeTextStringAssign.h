// AI CONTEXT: Centralizes low-level assignment into CommonLibF4 localized string storage.
// Depends on CommonLibF4 BGSLocalizedString declarations in plugin translation units.
// Runtime scope is Fallout 4 1.11.240 mutable string field storage.
// Version-specific logic: Fallout 4 1.11.240 only; no alternate runtime branches.
// Source-free policy: writes already-resolved destination text only; no lookup identity lives here.
#pragma once

#include <string>
#include <string_view>

namespace RE
{
	class BGSLocalizedString;
}

namespace Runtime111240::RuntimeTextStringAssign
{
	inline constexpr std::string_view kEmptyText{ " " };

	void AssignLocalized(RE::BGSLocalizedString& target, std::string_view text);
	void AssignPlainLocalized(RE::BGSLocalizedString& target, std::string_view text);

	[[nodiscard]] std::string NonEmptyString(std::string_view text);
}
