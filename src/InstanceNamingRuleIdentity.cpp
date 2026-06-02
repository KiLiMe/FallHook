// AI CONTEXT: Implements the shared hash for source-free INNR rule condition identity.
// Depends on InstanceNamingRuleIdentity declarations only.
// Runtime assumptions: version-neutral Fallout 4 naming-rule data loaded from plugins or runtime memory.
// Version-specific logic: none; the hash is a stable data-contract helper.
// Source-free policy: input values are keyword local FormIDs and rule indexes, not source text.
#include "InstanceNamingRuleIdentity.h"

namespace
{
	constexpr std::uint32_t kFnvOffset{ 2166136261u };
	constexpr std::uint32_t kFnvPrime{ 16777619u };

	void mix(std::uint32_t& hash, std::uint32_t value) noexcept
	{
		for (std::uint32_t i = 0; i < 4; ++i)
		{
			hash ^= (value >> (i * 8)) & 0xFFu;
			hash *= kFnvPrime;
		}
	}
}

namespace InstanceNamingRuleIdentity
{
	std::uint32_t HashSortedKeywords(std::span<const std::uint32_t> keywordLocalIDs, std::uint16_t ruleIndex) noexcept
	{
		auto hash = kFnvOffset;
		mix(hash, ruleIndex);
		mix(hash, static_cast<std::uint32_t>(keywordLocalIDs.size()));
		for (const auto keywordLocalID : keywordLocalIDs)
		{
			mix(hash, keywordLocalID & 0x00FFFFFFu);
		}
		return hash & 0x00FFFFFFu;
	}
}
