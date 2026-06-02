// AI CONTEXT: Parses Fallout 4 localized string IDs from CommonLibF4 string wrappers.
// Depends on RuntimeLocalizedStringID declarations and BGSLocalizedString string accessors.
// Runtime scope is Fallout 4 1.11.191 localized fields with <ID=XXXXXXXX> prefixes.
// Version-specific logic: none; string ID format is read-only.
// Source-free policy: the returned sID is identity metadata, not original text.
#include "PCH.h"

#include "111191/RuntimeLocalizedStringID.h"

#include <charconv>

namespace Runtime111191
{
namespace RuntimeLocalizedStringID
{
	std::optional<std::uint32_t> Parse(std::string_view text)
	{
		constexpr std::string_view kPrefix{ "<ID=" };
		constexpr std::size_t kHexDigits{ 8 };
		constexpr std::size_t kTotalPrefix{ 13 };
		if (text.size() < kTotalPrefix || !text.starts_with(kPrefix) || text[12] != '>')
		{
			return std::nullopt;
		}

		std::uint32_t parsed = 0;
		const auto id = text.substr(kPrefix.size(), kHexDigits);
		const auto result = std::from_chars(id.data(), id.data() + id.size(), parsed, 16);
		if (result.ec != std::errc{} || result.ptr != id.data() + id.size())
		{
			return std::nullopt;
		}
		return parsed;
	}

	std::optional<std::uint32_t> Read(const RE::BGSLocalizedString& text)
	{
		return Parse(std::string_view{ text.c_str(), text.length() });
	}
}

} // namespace Runtime111191
