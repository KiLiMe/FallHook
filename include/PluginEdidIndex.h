// AI CONTEXT: Plugin binary index for source-free EDID and string-ID resolution.
// Depends only on standard containers; implementation uses zlib for compressed records.
// Runtime assumptions: version-neutral Fallout 4 plugin record layouts.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: lookups use record signatures, raw form IDs, EDIDs, and string IDs only.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

class PluginEdidIndex
{
public:
	using RawFormID = std::uint32_t;

	static PluginEdidIndex Load(const std::filesystem::path& path, const std::unordered_set<std::uint32_t>& wantedSignatures);
	static std::uint32_t SignatureKey(std::string_view signature) noexcept;

	[[nodiscard]] bool loaded() const noexcept { return m_loaded; }
	[[nodiscard]] std::size_t entries() const noexcept { return m_edidToRawFormID.size(); }
	[[nodiscard]] std::size_t ambiguousEntries() const noexcept { return m_ambiguousKeys.size(); }
	[[nodiscard]] std::size_t recordsVisited() const noexcept { return m_recordsVisited; }
	[[nodiscard]] std::size_t recordsScanned() const noexcept { return m_recordsScanned; }
	[[nodiscard]] std::size_t recordsSkipped() const noexcept { return m_recordsSkipped; }
	[[nodiscard]] std::size_t compressedRecords() const noexcept { return m_compressedRecords; }
	[[nodiscard]] std::size_t failedCompressedRecords() const noexcept { return m_failedCompressedRecords; }

	[[nodiscard]] std::optional<RawFormID> lookup(std::string_view recordSignature, std::string_view edid) const;
	[[nodiscard]] std::optional<std::uint32_t> lookupQuestObjectiveIndex(RawFormID rawQuestFormID, std::uint32_t stringID) const;
	[[nodiscard]] std::optional<std::uint32_t> lookupQuestStageLogIndex(RawFormID rawQuestFormID, std::uint32_t stringID) const;
	[[nodiscard]] std::optional<std::uint32_t> lookupInfoResponseIndex(RawFormID rawInfoFormID, std::uint32_t stringID) const;
	[[nodiscard]] std::optional<std::uint32_t> lookupInfoResponseIDIndex(RawFormID rawInfoFormID, std::uint32_t responseID) const;
	[[nodiscard]] std::optional<std::uint32_t> lookupInfoPromptIndex(RawFormID rawInfoFormID, std::uint32_t stringID) const;
	[[nodiscard]] std::optional<RawFormID> lookupInfoParentGroup(RawFormID rawInfoFormID) const;
	[[nodiscard]] std::optional<std::uint32_t> lookupMessageButtonIndex(RawFormID rawMessageFormID, std::uint32_t stringID) const;
	[[nodiscard]] std::optional<std::uint32_t> lookupPerkActivateChoiceIndex(RawFormID rawPerkFormID, std::uint32_t stringID) const;
	[[nodiscard]] std::optional<std::uint32_t> lookupPerkTextIndex(RawFormID rawPerkFormID, std::uint32_t stringID) const;
	[[nodiscard]] std::optional<std::uint32_t> lookupTerminalBodyIndex(RawFormID rawTerminalFormID, std::uint32_t stringID) const;
	[[nodiscard]] std::optional<std::uint32_t> lookupTerminalItemIndex(RawFormID rawTerminalFormID, std::uint32_t stringID) const;
	[[nodiscard]] std::optional<std::uint32_t> lookupTerminalResponseIndex(RawFormID rawTerminalFormID, std::uint32_t stringID) const;
	[[nodiscard]] std::optional<std::uint32_t> lookupTerminalResultIndex(RawFormID rawTerminalFormID, std::uint32_t stringID) const;
	[[nodiscard]] std::optional<std::uint32_t> lookupInstanceNamingRuleSlot(RawFormID rawFormID, std::uint32_t stringID) const;
	[[nodiscard]] std::optional<std::uint32_t> lookupBodyPartNameSlot(RawFormID rawFormID, std::uint32_t stringID) const;
	[[nodiscard]] std::optional<std::uint32_t> lookupTemplateFullNameSlot(RawFormID rawFormID, std::uint32_t stringID) const;

	static constexpr std::uint32_t kInstanceNamingRuleSlotMarker = 0x80000000u;
	static constexpr std::uint32_t kInstanceNamingRuleConditionMarker = 0x20000000u;
	static constexpr std::uint32_t kTemplateFullNameSlotMarker = 0x40000000u;
	static constexpr std::uint32_t EncodeInstanceNamingRuleSlot(std::uint32_t ruleSet, std::uint32_t ruleOffset) noexcept
	{
		return kInstanceNamingRuleSlotMarker | ((ruleSet & 0xFFu) << 16) | (ruleOffset & 0xFFFFu);
	}
	static constexpr bool DecodeInstanceNamingRuleSlot(std::uint32_t encoded, std::uint32_t& ruleSet, std::uint32_t& ruleOffset) noexcept
	{
		if ((encoded & kInstanceNamingRuleSlotMarker) == 0 ||
			(encoded & kInstanceNamingRuleConditionMarker) != 0)
		{
			return false;
		}
		ruleSet = (encoded >> 16) & 0xFFu;
		ruleOffset = encoded & 0xFFFFu;
		return true;
	}
	static constexpr std::uint32_t EncodeInstanceNamingRuleConditionKey(std::uint32_t ruleSet, std::uint32_t conditionHash) noexcept
	{
		return kInstanceNamingRuleSlotMarker |
			kInstanceNamingRuleConditionMarker |
			((ruleSet & 0x0Fu) << 24) |
			(conditionHash & 0x00FFFFFFu);
	}
	static constexpr bool DecodeInstanceNamingRuleConditionKey(std::uint32_t encoded, std::uint32_t& ruleSet, std::uint32_t& conditionHash) noexcept
	{
		if ((encoded & kInstanceNamingRuleSlotMarker) == 0 ||
			(encoded & kInstanceNamingRuleConditionMarker) == 0 ||
			(encoded & kTemplateFullNameSlotMarker) != 0)
		{
			return false;
		}
		ruleSet = (encoded >> 24) & 0x0Fu;
		conditionHash = encoded & 0x00FFFFFFu;
		return true;
	}
	static constexpr std::uint32_t EncodeTemplateFullNameSlot(std::uint32_t slot) noexcept
	{
		return kTemplateFullNameSlotMarker | (slot & 0xFFFFu);
	}
	static constexpr bool DecodeTemplateFullNameSlot(std::uint32_t encoded, std::uint32_t& slot) noexcept
	{
		if ((encoded & kTemplateFullNameSlotMarker) == 0 || (encoded & kInstanceNamingRuleSlotMarker) != 0)
		{
			return false;
		}
		slot = encoded & 0xFFFFu;
		return true;
	}

private:
	void parse(std::span<const std::uint8_t> data);
	void parseRange(std::span<const std::uint8_t> data, std::size_t begin, std::size_t end);
	void parseRecord(std::string_view signature, RawFormID rawFormID, std::uint32_t flags, std::span<const std::uint8_t> data);
	void addEdid(std::string_view recordSignature, std::string_view edid, RawFormID rawFormID);

	std::unordered_map<std::string, RawFormID> m_edidToRawFormID;
	std::unordered_map<std::uint64_t, std::uint32_t> m_questObjectiveStringIDToIndex;
	std::unordered_map<std::uint64_t, std::uint32_t> m_questStageLogStringIDToIndex;
	std::unordered_map<std::uint64_t, std::uint32_t> m_infoResponseStringIDToIndex;
	std::unordered_map<std::uint64_t, std::uint32_t> m_infoResponseIDToIndex;
	std::unordered_map<std::uint64_t, std::uint32_t> m_infoPromptStringIDToIndex;
	std::unordered_map<RawFormID, RawFormID> m_infoParentGroupRawFormID;
	std::unordered_map<std::uint64_t, std::uint32_t> m_messageButtonStringIDToIndex;
	std::unordered_map<std::uint64_t, std::uint32_t> m_perkActivateChoiceStringIDToIndex;
	std::unordered_map<std::uint64_t, std::uint32_t> m_perkTextStringIDToIndex;
	std::unordered_map<std::uint64_t, std::uint32_t> m_terminalBodyStringIDToIndex;
	std::unordered_map<std::uint64_t, std::uint32_t> m_terminalItemStringIDToIndex;
	std::unordered_map<std::uint64_t, std::uint32_t> m_terminalResponseStringIDToIndex;
	std::unordered_map<std::uint64_t, std::uint32_t> m_terminalResultStringIDToIndex;
	std::unordered_map<std::uint64_t, std::uint32_t> m_instanceNamingStringIDToSlot;
	std::unordered_map<std::uint64_t, std::uint32_t> m_bodyPartStringIDToSlot;
	std::unordered_map<std::uint64_t, std::uint32_t> m_templateFullNameStringIDToSlot;
	std::unordered_set<std::uint32_t> m_wantedSignatures;
	std::unordered_set<std::string> m_ambiguousKeys;
	std::size_t m_recordsVisited{ 0 };
	std::size_t m_recordsScanned{ 0 };
	std::size_t m_recordsSkipped{ 0 };
	std::size_t m_compressedRecords{ 0 };
	std::size_t m_failedCompressedRecords{ 0 };
	bool m_loaded{ false };
};
