// AI CONTEXT: Plugin binary index for source-free record identity.
// Depends on PluginEdidIndex declarations and zlib for Fallout 4 compressed records.
// Runtime assumptions: version-neutral Fallout 4 plugin file structure.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: original text subrecords are ignored; identity comes from EDID/form/string IDs.
#include "PluginEdidIndex.h"

#include "PluginEdidIndexInnr.h"

#include <zlib.h>

#include <fstream>
#include <vector>

namespace
{
	constexpr std::size_t RECORD_HEADER_SIZE = 24;
	constexpr std::uint32_t RECORD_FLAG_DELETED = 0x00000020;
	constexpr std::uint32_t RECORD_FLAG_COMPRESSED = 0x00040000;
	constexpr std::size_t BPND_TYPE_OFFSET = 0x41;
	constexpr std::uint8_t MAX_BODY_PART_SLOT = 25;

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

	std::uint64_t compactKey(PluginEdidIndex::RawFormID rawFormID, std::uint32_t value)
	{
		return (static_cast<std::uint64_t>(rawFormID) << 32) | value;
	}

	void addIndexedValue(
		std::unordered_map<std::uint64_t, std::uint32_t>& map,
		PluginEdidIndex::RawFormID rawFormID,
		std::uint32_t value,
		std::uint32_t index)
	{
		if (rawFormID == 0 || value == 0)
		{
			return;
		}

		map.emplace(compactKey(rawFormID, value), index);
		map.emplace(compactKey(rawFormID & 0x00FFFFFF, value), index);
		map.emplace(compactKey(rawFormID & 0x00000FFF, value), index);
	}

	void addRawFormMapping(
		std::unordered_map<PluginEdidIndex::RawFormID, PluginEdidIndex::RawFormID>& map,
		PluginEdidIndex::RawFormID rawFormID,
		PluginEdidIndex::RawFormID targetRawFormID)
	{
		if (rawFormID == 0 || targetRawFormID == 0)
		{
			return;
		}

		map.emplace(rawFormID, targetRawFormID);
		map.emplace(rawFormID & 0x00FFFFFF, targetRawFormID);
		map.emplace(rawFormID & 0x00000FFF, targetRawFormID);
	}

	bool decompressRecordData(std::span<const std::uint8_t> input, std::vector<std::uint8_t>& output)
	{
		if (input.size() < sizeof(std::uint32_t))
		{
			return false;
		}

		const auto uncompressedSize = readU32(input, 0);
		if (uncompressedSize == 0)
		{
			return false;
		}

		output.resize(uncompressedSize);
		auto outputSize = static_cast<uLongf>(output.size());
		const auto result = ::uncompress(
			output.data(),
			&outputSize,
			input.data() + sizeof(std::uint32_t),
			static_cast<uLong>(input.size() - sizeof(std::uint32_t)));
		if (result != Z_OK || outputSize != uncompressedSize)
		{
			output.clear();
			return false;
		}

		return true;
	}

	std::optional<std::string> findEdid(std::span<const std::uint8_t> data)
	{
		std::size_t offset = 0;
		std::uint32_t extendedSize = 0;
		while (offset + 6 <= data.size())
		{
			const auto signature = readSignature(data, offset);
			const auto size = readU16(data, offset + 4);
			offset += 6;

			if (signature == "XXXX")
			{
				if (size != sizeof(std::uint32_t) || offset + size > data.size())
				{
					return std::nullopt;
				}
				extendedSize = readU32(data, offset);
				offset += size;
				continue;
			}

			const auto subrecordSize = extendedSize != 0 ? extendedSize : static_cast<std::uint32_t>(size);
			extendedSize = 0;
			if (offset + subrecordSize > data.size())
			{
				return std::nullopt;
			}

			if (signature == "EDID")
			{
				const auto* begin = reinterpret_cast<const char*>(data.data() + offset);
				std::size_t length = 0;
				while (length < subrecordSize && begin[length] != '\0')
				{
					++length;
				}
				return length == 0 ? std::nullopt : std::optional<std::string>{ std::string{ begin, length } };
			}

			offset += subrecordSize;
		}

		return std::nullopt;
	}

	struct SubrecordCursor
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

	std::uint32_t readPerkFragmentIndex(std::span<const std::uint8_t> data)
	{
		if (data.size() < sizeof(std::uint32_t))
		{
			return 0;
		}

		const auto low = readU16(data, 0);
		const auto high = readU16(data, 2);
		return high != 0 ? high : low;
	}
}

PluginEdidIndex PluginEdidIndex::Load(const std::filesystem::path& path, const std::unordered_set<std::uint32_t>& wantedSignatures)
{
	PluginEdidIndex index;
	index.m_wantedSignatures = wantedSignatures;

	std::ifstream input(path, std::ios::binary | std::ios::ate);
	if (!input)
	{
		return index;
	}

	const auto size = input.tellg();
	if (size <= 0)
	{
		return index;
	}

	std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
	input.seekg(0, std::ios::beg);
	if (!input.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size())))
	{
		return index;
	}

	index.m_loaded = true;
	if (!index.m_wantedSignatures.empty())
	{
		index.parse(data);
	}
	return index;
}

std::uint32_t PluginEdidIndex::SignatureKey(std::string_view signature) noexcept
{
	if (signature.size() != 4)
	{
		return 0;
	}

	return static_cast<std::uint32_t>(static_cast<unsigned char>(signature[0])) |
		(static_cast<std::uint32_t>(static_cast<unsigned char>(signature[1])) << 8) |
		(static_cast<std::uint32_t>(static_cast<unsigned char>(signature[2])) << 16) |
		(static_cast<std::uint32_t>(static_cast<unsigned char>(signature[3])) << 24);
}

void PluginEdidIndex::parse(std::span<const std::uint8_t> data)
{
	parseRange(data, 0, data.size());
}

void PluginEdidIndex::parseRange(std::span<const std::uint8_t> data, std::size_t begin, std::size_t end)
{
	if (begin > data.size())
	{
		return;
	}

	end = std::min(end, data.size());
	auto offset = begin;
	while (offset + RECORD_HEADER_SIZE <= end)
	{
		const auto signature = readSignature(data, offset);
		const auto size = readU32(data, offset + 4);
		if (signature.empty() || size == 0)
		{
			return;
		}

		if (signature == "GRUP")
		{
			if (size < RECORD_HEADER_SIZE || offset + size > end)
			{
				return;
			}
			parseRange(data, offset + RECORD_HEADER_SIZE, offset + size);
			offset += size;
			continue;
		}

		const auto recordDataBegin = offset + RECORD_HEADER_SIZE;
		const auto recordDataEnd = recordDataBegin + size;
		if (recordDataEnd > end || recordDataEnd < recordDataBegin)
		{
			return;
		}

		++m_recordsVisited;
		const auto flags = readU32(data, offset + 8);
		if ((flags & RECORD_FLAG_DELETED) == 0 && m_wantedSignatures.contains(SignatureKey(signature)))
		{
			++m_recordsScanned;
			parseRecord(signature, readU32(data, offset + 12), flags, data.subspan(recordDataBegin, size));
		}
		else
		{
			++m_recordsSkipped;
		}

		offset = recordDataEnd;
	}
}

void PluginEdidIndex::parseRecord(std::string_view signature, RawFormID rawFormID, std::uint32_t flags, std::span<const std::uint8_t> data)
{
	if (signature.size() != 4 || rawFormID == 0)
	{
		return;
	}

	std::vector<std::uint8_t> decompressed;
	auto recordData = data;
	if ((flags & RECORD_FLAG_COMPRESSED) != 0)
	{
		++m_compressedRecords;
		if (!decompressRecordData(data, decompressed))
		{
			++m_failedCompressedRecords;
			return;
		}
		recordData = std::span<const std::uint8_t>{ decompressed.data(), decompressed.size() };
	}

	if (const auto edid = findEdid(recordData); edid && !edid->empty())
	{
		addEdid(signature, *edid, rawFormID);
	}

	std::string_view subSig;
	std::span<const std::uint8_t> payload;
	SubrecordCursor cursor{ recordData };
	std::uint32_t responseOrdinal = 0;
	std::uint32_t buttonIndex = 0;
	std::uint32_t terminalBody = 0;
	std::uint32_t terminalItem = 0;
	std::uint32_t terminalResponse = 0;
	std::uint32_t terminalResult = 0;
	std::uint32_t currentStageItemOrdinal = 0;
	std::uint32_t currentStageItemIndex = 0;
	std::uint32_t textFunctionIndex = 0;
	std::uint32_t fullNameOrdinal = 0;
	std::optional<std::uint32_t> currentObjectiveIndex;
	std::optional<std::uint32_t> currentStageIndex;
	std::optional<std::uint32_t> pendingResponseID;
	std::optional<std::uint32_t> lastFunctionType;
	std::optional<std::uint32_t> pendingActivateChoiceStringID;
	std::optional<std::uint32_t> pendingBodyPartStringID;
	bool hasCurrentStageItem = false;

	if (signature == "INNR")
	{
		for (const auto& key : PluginEdidIndexInnr::ExtractRuleKeys(recordData))
		{
			addIndexedValue(m_instanceNamingStringIDToSlot, rawFormID, key.stringID, key.encodedKey);
		}
	}

	while (cursor.next(subSig, payload))
	{
		if (signature == "BPTD" && subSig == "BPTN" && payload.size() == sizeof(std::uint32_t))
		{
			pendingBodyPartStringID = readU32(payload, 0);
		}
		else if (signature == "BPTD" && subSig == "BPND" && pendingBodyPartStringID &&
			payload.size() > BPND_TYPE_OFFSET && payload[BPND_TYPE_OFFSET] <= MAX_BODY_PART_SLOT)
		{
			addIndexedValue(m_bodyPartStringIDToSlot, rawFormID, *pendingBodyPartStringID, payload[BPND_TYPE_OFFSET]);
			pendingBodyPartStringID.reset();
		}
		else if ((signature == "WEAP" || signature == "ARMO") && subSig == "FULL" && payload.size() == sizeof(std::uint32_t))
		{
			const auto stringID = readU32(payload, 0);
			if (fullNameOrdinal > 0)
			{
				addIndexedValue(
					m_templateFullNameStringIDToSlot,
					rawFormID,
					stringID,
					PluginEdidIndex::EncodeTemplateFullNameSlot(fullNameOrdinal - 1));
			}
			++fullNameOrdinal;
		}
		else if (signature == "QUST" && subSig == "QOBJ" && payload.size() >= sizeof(std::uint16_t))
		{
			currentObjectiveIndex = readU16(payload, 0);
		}
		else if (signature == "QUST" && subSig == "INDX" && payload.size() >= sizeof(std::uint16_t))
		{
			currentStageIndex = readU16(payload, 0);
			currentStageItemOrdinal = 0;
			hasCurrentStageItem = false;
		}
		else if (signature == "QUST" && subSig == "QSDT")
		{
			currentStageItemIndex = currentStageItemOrdinal++;
			hasCurrentStageItem = true;
		}
		else if (signature == "QUST" && subSig == "NNAM" && currentObjectiveIndex && payload.size() == sizeof(std::uint32_t))
		{
			addIndexedValue(m_questObjectiveStringIDToIndex, rawFormID, readU32(payload, 0), *currentObjectiveIndex);
		}
		else if (signature == "QUST" && subSig == "CNAM" && currentStageIndex && hasCurrentStageItem && payload.size() == sizeof(std::uint32_t))
		{
			addIndexedValue(m_questStageLogStringIDToIndex, rawFormID, readU32(payload, 0), *currentStageIndex + currentStageItemIndex);
		}
		else if (signature == "INFO" && subSig == "TRDT" && payload.size() > 12)
		{
			pendingResponseID = payload[12];
		}
		else if (signature == "INFO" && subSig == "NAM1" && payload.size() == sizeof(std::uint32_t))
		{
			if (pendingResponseID && *pendingResponseID != 0)
			{
				addIndexedValue(m_infoResponseIDToIndex, rawFormID, *pendingResponseID, responseOrdinal);
			}
			pendingResponseID.reset();
			addIndexedValue(m_infoResponseStringIDToIndex, rawFormID, readU32(payload, 0), responseOrdinal++);
		}
		else if (signature == "INFO" && subSig == "RNAM" && payload.size() == sizeof(std::uint32_t))
		{
			addIndexedValue(m_infoPromptStringIDToIndex, rawFormID, readU32(payload, 0), 0);
		}
		else if (signature == "INFO" && subSig == "GNAM" && payload.size() == sizeof(std::uint32_t))
		{
			addRawFormMapping(m_infoParentGroupRawFormID, rawFormID, readU32(payload, 0));
		}
		else if (signature == "MESG" && subSig == "ITXT" && payload.size() == sizeof(std::uint32_t))
		{
			addIndexedValue(m_messageButtonStringIDToIndex, rawFormID, readU32(payload, 0), buttonIndex++);
		}
		else if (signature == "PERK" && subSig == "EPFT" && !payload.empty())
		{
			lastFunctionType = payload[0];
			pendingActivateChoiceStringID.reset();
		}
		else if (signature == "PERK" && lastFunctionType == 4 && subSig == "EPF2" && payload.size() == sizeof(std::uint32_t))
		{
			pendingActivateChoiceStringID = readU32(payload, 0);
		}
		else if (signature == "PERK" && lastFunctionType == 4 && subSig == "EPF3" && pendingActivateChoiceStringID)
		{
			addIndexedValue(m_perkActivateChoiceStringIDToIndex, rawFormID, *pendingActivateChoiceStringID, readPerkFragmentIndex(payload));
			pendingActivateChoiceStringID.reset();
		}
		else if (signature == "PERK" && lastFunctionType == 7 && subSig == "EPFD" && payload.size() == sizeof(std::uint32_t))
		{
			addIndexedValue(m_perkTextStringIDToIndex, rawFormID, readU32(payload, 0), textFunctionIndex++);
		}
		else if (signature == "TERM" && subSig == "BTXT" && payload.size() == sizeof(std::uint32_t))
		{
			addIndexedValue(m_terminalBodyStringIDToIndex, rawFormID, readU32(payload, 0), terminalBody++);
		}
		else if (signature == "TERM" && subSig == "ITXT" && payload.size() == sizeof(std::uint32_t))
		{
			addIndexedValue(m_terminalItemStringIDToIndex, rawFormID, readU32(payload, 0), terminalItem++);
		}
		else if (signature == "TERM" && subSig == "RNAM" && payload.size() == sizeof(std::uint32_t))
		{
			addIndexedValue(m_terminalResponseStringIDToIndex, rawFormID, readU32(payload, 0), terminalResponse++);
		}
		else if (signature == "TERM" && subSig == "UNAM" && payload.size() == sizeof(std::uint32_t))
		{
			addIndexedValue(m_terminalResultStringIDToIndex, rawFormID, readU32(payload, 0), terminalResult++);
		}
	}
}
