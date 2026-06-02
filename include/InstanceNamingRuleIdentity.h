// AI CONTEXT: Shared source-free identity hash for INNR rule conditions.
// Depends only on standard integer/span types so plugin parsing and runtime code can share it.
// Runtime assumptions: version-neutral Fallout 4 BGSInstanceNamingRules rule condition identity.
// Version-specific logic: none; callers provide Fallout 4 local keyword IDs and rule indexes.
// Source-free policy: hashes keyword/form identity and rule metadata only; never hashes display text.
#pragma once

#include <cstdint>
#include <span>

namespace InstanceNamingRuleIdentity
{
	std::uint32_t HashSortedKeywords(std::span<const std::uint32_t> keywordLocalIDs, std::uint16_t ruleIndex) noexcept;
}
