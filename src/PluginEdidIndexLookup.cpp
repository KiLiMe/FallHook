// AI CONTEXT: Lookup and EDID-key methods for PluginEdidIndex.
// Depends only on PluginEdidIndex internal maps and standard normalization helpers.
// Runtime assumptions: version-neutral Fallout 4 plugin identity.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: lookup keys contain record signatures, EDIDs, raw form IDs, and string IDs only.
#include "PluginEdidIndex.h"

#include <algorithm>
#include <cctype>

namespace
{
	std::string normalizedRecordSignature(std::string_view signature)
	{
		std::string result{ signature.substr(0, std::min<std::size_t>(signature.size(), 4)) };
		std::ranges::transform(result, result.begin(), [](unsigned char ch) {
			return static_cast<char>(std::toupper(ch));
		});
		return result;
	}

	std::string normalizedEdid(std::string_view edid)
	{
		const auto begin = edid.find_first_not_of(" \t\r\n");
		if (begin == std::string_view::npos)
		{
			return {};
		}

		const auto end = edid.find_last_not_of(" \t\r\n");
		std::string result{ edid.substr(begin, end - begin + 1) };
		std::ranges::transform(result, result.begin(), [](unsigned char ch) {
			return static_cast<char>(std::tolower(ch));
		});
		return result;
	}

	std::string makeEdidKey(std::string_view recordSignature, std::string_view edid)
	{
		return normalizedRecordSignature(recordSignature) + '|' + normalizedEdid(edid);
	}

	std::uint64_t compactKey(PluginEdidIndex::RawFormID rawFormID, std::uint32_t value)
	{
		return (static_cast<std::uint64_t>(rawFormID) << 32) | value;
	}

	std::optional<std::uint32_t> lookupIndexedValue(
		const std::unordered_map<std::uint64_t, std::uint32_t>& map,
		PluginEdidIndex::RawFormID rawFormID,
		std::uint32_t value)
	{
		if (rawFormID == 0 || value == 0)
		{
			return std::nullopt;
		}

		const auto it = map.find(compactKey(rawFormID, value));
		if (it == map.end())
		{
			return std::nullopt;
		}

		return it->second;
	}
}

std::optional<PluginEdidIndex::RawFormID> PluginEdidIndex::lookup(std::string_view recordSignature, std::string_view edid) const
{
	if (!m_loaded || recordSignature.size() != 4 || edid.empty())
	{
		return std::nullopt;
	}

	const auto key = makeEdidKey(recordSignature, edid);
	if (m_ambiguousKeys.contains(key))
	{
		return std::nullopt;
	}

	const auto it = m_edidToRawFormID.find(key);
	if (it == m_edidToRawFormID.end())
	{
		return std::nullopt;
	}

	return it->second;
}

void PluginEdidIndex::addEdid(std::string_view recordSignature, std::string_view edid, RawFormID rawFormID)
{
	const auto key = makeEdidKey(recordSignature, edid);
	if (m_ambiguousKeys.contains(key))
	{
		return;
	}

	const auto [it, inserted] = m_edidToRawFormID.emplace(key, rawFormID);
	if (!inserted && it->second != rawFormID)
	{
		m_edidToRawFormID.erase(key);
		m_ambiguousKeys.emplace(key);
	}
}

std::optional<std::uint32_t> PluginEdidIndex::lookupQuestObjectiveIndex(RawFormID rawQuestFormID, std::uint32_t stringID) const
{
	return m_loaded ? lookupIndexedValue(m_questObjectiveStringIDToIndex, rawQuestFormID, stringID) : std::nullopt;
}

std::optional<std::uint32_t> PluginEdidIndex::lookupQuestStageLogIndex(RawFormID rawQuestFormID, std::uint32_t stringID) const
{
	return m_loaded ? lookupIndexedValue(m_questStageLogStringIDToIndex, rawQuestFormID, stringID) : std::nullopt;
}

std::optional<std::uint32_t> PluginEdidIndex::lookupInfoResponseIndex(RawFormID rawInfoFormID, std::uint32_t stringID) const
{
	return m_loaded ? lookupIndexedValue(m_infoResponseStringIDToIndex, rawInfoFormID, stringID) : std::nullopt;
}

std::optional<std::uint32_t> PluginEdidIndex::lookupInfoResponseIDIndex(RawFormID rawInfoFormID, std::uint32_t responseID) const
{
	return m_loaded ? lookupIndexedValue(m_infoResponseIDToIndex, rawInfoFormID, responseID) : std::nullopt;
}

std::optional<std::uint32_t> PluginEdidIndex::lookupInfoPromptIndex(RawFormID rawInfoFormID, std::uint32_t stringID) const
{
	return m_loaded ? lookupIndexedValue(m_infoPromptStringIDToIndex, rawInfoFormID, stringID) : std::nullopt;
}

std::optional<PluginEdidIndex::RawFormID> PluginEdidIndex::lookupInfoParentGroup(RawFormID rawInfoFormID) const
{
	if (!m_loaded || rawInfoFormID == 0)
	{
		return std::nullopt;
	}

	const auto it = m_infoParentGroupRawFormID.find(rawInfoFormID);
	return it == m_infoParentGroupRawFormID.end() ? std::nullopt : std::optional<RawFormID>{ it->second };
}

std::optional<std::uint32_t> PluginEdidIndex::lookupMessageButtonIndex(RawFormID rawMessageFormID, std::uint32_t stringID) const
{
	return m_loaded ? lookupIndexedValue(m_messageButtonStringIDToIndex, rawMessageFormID, stringID) : std::nullopt;
}

std::optional<std::uint32_t> PluginEdidIndex::lookupPerkActivateChoiceIndex(RawFormID rawPerkFormID, std::uint32_t stringID) const
{
	return m_loaded ? lookupIndexedValue(m_perkActivateChoiceStringIDToIndex, rawPerkFormID, stringID) : std::nullopt;
}

std::optional<std::uint32_t> PluginEdidIndex::lookupPerkTextIndex(RawFormID rawPerkFormID, std::uint32_t stringID) const
{
	return m_loaded ? lookupIndexedValue(m_perkTextStringIDToIndex, rawPerkFormID, stringID) : std::nullopt;
}

std::optional<std::uint32_t> PluginEdidIndex::lookupTerminalBodyIndex(RawFormID rawTerminalFormID, std::uint32_t stringID) const
{
	return m_loaded ? lookupIndexedValue(m_terminalBodyStringIDToIndex, rawTerminalFormID, stringID) : std::nullopt;
}

std::optional<std::uint32_t> PluginEdidIndex::lookupTerminalItemIndex(RawFormID rawTerminalFormID, std::uint32_t stringID) const
{
	return m_loaded ? lookupIndexedValue(m_terminalItemStringIDToIndex, rawTerminalFormID, stringID) : std::nullopt;
}

std::optional<std::uint32_t> PluginEdidIndex::lookupTerminalResponseIndex(RawFormID rawTerminalFormID, std::uint32_t stringID) const
{
	return m_loaded ? lookupIndexedValue(m_terminalResponseStringIDToIndex, rawTerminalFormID, stringID) : std::nullopt;
}

std::optional<std::uint32_t> PluginEdidIndex::lookupTerminalResultIndex(RawFormID rawTerminalFormID, std::uint32_t stringID) const
{
	return m_loaded ? lookupIndexedValue(m_terminalResultStringIDToIndex, rawTerminalFormID, stringID) : std::nullopt;
}

std::optional<std::uint32_t> PluginEdidIndex::lookupInstanceNamingRuleSlot(RawFormID rawFormID, std::uint32_t stringID) const
{
	return m_loaded ? lookupIndexedValue(m_instanceNamingStringIDToSlot, rawFormID, stringID) : std::nullopt;
}

std::optional<std::uint32_t> PluginEdidIndex::lookupBodyPartNameSlot(RawFormID rawFormID, std::uint32_t stringID) const
{
	return m_loaded ? lookupIndexedValue(m_bodyPartStringIDToSlot, rawFormID, stringID) : std::nullopt;
}

std::optional<std::uint32_t> PluginEdidIndex::lookupTemplateFullNameSlot(RawFormID rawFormID, std::uint32_t stringID) const
{
	return m_loaded ? lookupIndexedValue(m_templateFullNameStringIDToSlot, rawFormID, stringID) : std::nullopt;
}
