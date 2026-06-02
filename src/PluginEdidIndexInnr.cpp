// AI CONTEXT: Parses INNR rule subrecords into source-free condition keys.
// Depends on PluginEdidIndex encoding and InstanceNamingRuleIdentity hashing.
// Runtime assumptions: version-neutral Fallout 4 INNR WNAM/KSIZ/KWDA/YNAM layout.
// Version-specific logic: none; parses Fallout 4 plugin data without executable offsets.
// Source-free policy: uses WNAM sID and rule condition data only; never reads source text.
#include "PluginEdidIndexInnr.h"

#include "InstanceNamingRuleIdentity.h"
#include "PluginEdidIndex.h"

#include <algorithm>
#include <optional>

namespace
{
	std::uint16_t readU16(std::span<const std::uint8_t> data, std::size_t offset)
	{
		if (offset + sizeof(std::uint16_t) > data.size())
		{
			return 0;
		}
		return static_cast<std::uint16_t>(data[offset]) |
			(static_cast<std::uint16_t>(data[offset + 1]) << 8);
	}

	std::uint32_t readU32(std::span<const std::uint8_t> data, std::size_t offset)
	{
		if (offset + sizeof(std::uint32_t) > data.size())
		{
			return 0;
		}
		return static_cast<std::uint32_t>(data[offset]) |
			(static_cast<std::uint32_t>(data[offset + 1]) << 8) |
			(static_cast<std::uint32_t>(data[offset + 2]) << 16) |
			(static_cast<std::uint32_t>(data[offset + 3]) << 24);
	}

	std::string_view readSignature(std::span<const std::uint8_t> data, std::size_t offset)
	{
		if (offset + 4 > data.size())
		{
			return {};
		}
		return std::string_view{ reinterpret_cast<const char*>(data.data() + offset), 4 };
	}

	struct Cursor
	{
		std::span<const std::uint8_t> data;
		std::size_t offset{ 0 };
		std::uint32_t extendedSize{ 0 };

		bool next(std::string_view& signature, std::span<const std::uint8_t>& payload)
		{
			if (offset + 6 > data.size())
			{
				return false;
			}
			signature = readSignature(data, offset);
			const auto size = readU16(data, offset + 4);
			offset += 6;
			if (signature == "XXXX")
			{
				if (size != sizeof(std::uint32_t) || offset + size > data.size())
				{
					return false;
				}
				extendedSize = readU32(data, offset);
				offset += size;
				return next(signature, payload);
			}
			const auto subrecordSize = extendedSize != 0 ? extendedSize : static_cast<std::uint32_t>(size);
			extendedSize = 0;
			if (offset + subrecordSize > data.size())
			{
				return false;
			}
			payload = data.subspan(offset, subrecordSize);
			offset += subrecordSize;
			return true;
		}
	};

	struct RuleBuilder
	{
		std::uint32_t stringID{ 0 };
		std::uint16_t ruleIndex{ 0 };
		std::vector<std::uint32_t> keywordLocalIDs;
	};

	void flushRule(
		std::vector<PluginEdidIndexInnr::RuleKey>& out,
		std::uint32_t ruleSet,
		std::optional<RuleBuilder>& rule)
	{
		if (!rule || ruleSet >= 10)
		{
			rule.reset();
			return;
		}
		std::ranges::sort(rule->keywordLocalIDs);
		const auto hash = InstanceNamingRuleIdentity::HashSortedKeywords(rule->keywordLocalIDs, rule->ruleIndex);
		out.push_back(PluginEdidIndexInnr::RuleKey{
			.stringID = rule->stringID,
			.encodedKey = PluginEdidIndex::EncodeInstanceNamingRuleConditionKey(ruleSet, hash)
		});
		rule.reset();
	}
}

namespace PluginEdidIndexInnr
{
	std::vector<RuleKey> ExtractRuleKeys(std::span<const std::uint8_t> recordData)
	{
		std::vector<RuleKey> keys;
		std::uint32_t ruleSet = 0xFFFFFFFFu;
		std::optional<RuleBuilder> rule;
		Cursor cursor{ recordData };
		std::string_view signature;
		std::span<const std::uint8_t> payload;
		while (cursor.next(signature, payload))
		{
			if (signature == "VNAM")
			{
				flushRule(keys, ruleSet, rule);
				ruleSet = ruleSet == 0xFFFFFFFFu ? 0 : ruleSet + 1;
			}
			else if (signature == "WNAM" && payload.size() == sizeof(std::uint32_t))
			{
				flushRule(keys, ruleSet, rule);
				rule = RuleBuilder{ .stringID = readU32(payload, 0) };
			}
			else if (rule && signature == "KWDA" && payload.size() % sizeof(std::uint32_t) == 0)
			{
				for (std::size_t offset = 0; offset < payload.size(); offset += sizeof(std::uint32_t))
				{
					rule->keywordLocalIDs.push_back(readU32(payload, offset) & 0x00FFFFFFu);
				}
			}
			else if (rule && signature == "YNAM" && payload.size() >= sizeof(std::uint16_t))
			{
				rule->ruleIndex = readU16(payload, 0);
			}
		}
		flushRule(keys, ruleSet, rule);
		return keys;
	}
}
