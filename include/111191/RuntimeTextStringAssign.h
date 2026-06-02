// AI CONTEXT: Centralizes low-level assignment into CommonLibF4 localized string storage.
// Depends on CommonLibF4 BGSLocalizedString declarations in plugin translation units.
// Runtime scope is Fallout 4 1.11.191 mutable string field storage.
// Version-specific logic: Fallout 4 1.11.191 only; no alternate runtime branches.
// Source-free policy: writes already-resolved destination text only; no lookup identity lives here.
#pragma once

#include "RE/B/BSFixedString.h"

#include <string>
#include <string_view>

namespace RE
{
	class BGSLocalizedString;
}

namespace Runtime111191::RuntimeTextStringAssign
{
	inline constexpr std::string_view kEmptyText{ " " };

	void AssignLocalized(RE::BGSLocalizedString& target, std::string_view text);
	void AssignPlainLocalized(RE::BGSLocalizedString& target, std::string_view text);
	void AssignPlainFixedLocalized(RE::BGSLocalizedString& target, const RE::BSFixedStringCS& text);

	[[nodiscard]] std::string NonEmptyString(std::string_view text);
}
