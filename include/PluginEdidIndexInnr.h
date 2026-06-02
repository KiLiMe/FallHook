// AI CONTEXT: Extracts source-free INNR rule condition keys from plugin record bytes.
// Depends on PluginEdidIndex encoding and shared INNR rule identity hashing.
// Runtime assumptions: version-neutral Fallout 4 INNR subrecord layout.
// Version-specific logic: none; parses Fallout 4 plugin data without executable offsets.
// Source-free policy: maps WNAM sID to keyword/rule condition identity; never reads source text.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace PluginEdidIndexInnr
{
	struct RuleKey
	{
		std::uint32_t stringID{ 0 };
		std::uint32_t encodedKey{ 0 };
	};

	std::vector<RuleKey> ExtractRuleKeys(std::span<const std::uint8_t> recordData);
}
