// AI CONTEXT: Source-free translation catalog builder.
// Depends on XML mapping, runtime semantic indexes, destination normalization, and load-order sorting.
// Runtime assumptions: version-neutral Fallout 4 translation records.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: original Source text is ignored; identity uses XML metadata such as sID/EDID/REC.
#include "TranslationCatalog.h"

#include "RuntimeResolution.h"
#include "TextNormalization.h"
#include "XmlTranslationMapping.h"

#include <cctype>
#include <unordered_map>

namespace
{
	bool isBlank(std::string_view value)
	{
		for (const auto ch : value)
		{
			if (!std::isspace(static_cast<unsigned char>(ch)))
			{
				return false;
			}
		}
		return true;
	}

	std::string fileSortName(const XmlTranslationFile& file)
	{
		if (!file.path.empty())
		{
			return file.path.string();
		}

		return file.addon;
	}

	std::optional<std::string> editorIdentity(const XmlTranslationEntry& entry)
	{
		if (entry.edid.empty() || XmlTranslationMapping::ParseBracketFormID(entry.edid))
		{
			return std::nullopt;
		}

		return entry.edid;
	}

	std::optional<std::uint32_t> resolveFormID(const TranslationCatalogFile& file, const XmlTranslationEntry& entry, std::string_view recordSignature)
	{
		if (auto bracketFormID = XmlTranslationMapping::ParseBracketFormID(entry.edid))
		{
			return bracketFormID;
		}

		if (!file.pluginIndex || entry.edid.empty() || recordSignature.size() < 4)
		{
			return std::nullopt;
		}

		return file.pluginIndex->lookup(recordSignature.substr(0, 4), entry.edid);
	}

	std::optional<std::uint32_t> resolvePluginStringIndex(
		const TranslationCatalogFile& file,
		const XmlTranslationEntry& entry,
		std::optional<std::uint32_t> formID,
		std::string_view recordSignature)
	{
		if (!file.pluginIndex || !entry.stringID || !formID)
		{
			return std::nullopt;
		}

		if (recordSignature == "INNR WNAM")
		{
			return file.pluginIndex->lookupInstanceNamingRuleSlot(*formID, *entry.stringID);
		}
		if (recordSignature == "BPTD BPTN")
		{
			return file.pluginIndex->lookupBodyPartNameSlot(*formID, *entry.stringID);
		}
		if (recordSignature == "WEAP FULL" || recordSignature == "ARMO FULL")
		{
			return file.pluginIndex->lookupTemplateFullNameSlot(*formID, *entry.stringID);
		}
		if (recordSignature == "QUST NNAM")
		{
			return file.pluginIndex->lookupQuestObjectiveIndex(*formID, *entry.stringID);
		}
		if (recordSignature == "QUST CNAM")
		{
			return file.pluginIndex->lookupQuestStageLogIndex(*formID, *entry.stringID);
		}
		if (recordSignature == "INFO NAM1")
		{
			return file.pluginIndex->lookupInfoResponseIndex(*formID, *entry.stringID);
		}
		if (recordSignature == "INFO RNAM")
		{
			return file.pluginIndex->lookupInfoPromptIndex(*formID, *entry.stringID);
		}
		if (recordSignature == "MESG ITXT")
		{
			return file.pluginIndex->lookupMessageButtonIndex(*formID, *entry.stringID);
		}
		if (recordSignature == "PERK EPF2")
		{
			return file.pluginIndex->lookupPerkActivateChoiceIndex(*formID, *entry.stringID);
		}
		if (recordSignature == "PERK EPFD")
		{
			return file.pluginIndex->lookupPerkTextIndex(*formID, *entry.stringID);
		}
		if (recordSignature == "TERM BTXT")
		{
			return file.pluginIndex->lookupTerminalBodyIndex(*formID, *entry.stringID);
		}
		if (recordSignature == "TERM ITXT")
		{
			return file.pluginIndex->lookupTerminalItemIndex(*formID, *entry.stringID);
		}
		if (recordSignature == "TERM RNAM")
		{
			return file.pluginIndex->lookupTerminalResponseIndex(*formID, *entry.stringID);
		}
		if (recordSignature == "TERM UNAM")
		{
			return file.pluginIndex->lookupTerminalResultIndex(*formID, *entry.stringID);
		}

		return std::nullopt;
	}

	std::optional<std::uint32_t> semanticIndex(const XmlTranslationEntry& entry, TranslationType type, std::string_view recordSignature)
	{
		if (recordSignature == "BPTD BPTN")
		{
			return std::nullopt;
		}

		return RuntimeResolution::GetSemanticIndex(entry, type);
	}

	SourceFreeTranslationKey makeIdentity(
		const TranslationCatalogFile& file,
		const XmlTranslationEntry& entry,
		TranslationType type,
		std::string_view recordSignature,
		bool* usedPluginStringID = nullptr)
	{
		SourceFreeTranslationKey key;
		key.pluginName = file.file.addon;
		key.formID = resolveFormID(file, entry, recordSignature);
		key.editorID = key.formID ? std::nullopt : editorIdentity(entry);
		key.type = type;
		key.index = semanticIndex(entry, type, recordSignature);
		if (auto stringIndex = resolvePluginStringIndex(file, entry, key.formID, recordSignature))
		{
			key.index = stringIndex;
			if (usedPluginStringID)
			{
				*usedPluginStringID = true;
			}
		}
		key.stringID = entry.stringID;
		return key;
	}

	bool hasIdentity(const SourceFreeTranslationKey& key)
	{
		return key.formID.has_value() || key.editorID.has_value() || key.stringID.has_value();
	}

	TranslationCatalogRecord makeRecord(
		const TranslationCatalogFile& file,
		const XmlTranslationEntry& entry,
		TranslationType type,
		const std::string& recordSignature,
		TranslationCatalogBuildResult& result)
	{
		bool usedPluginStringID = false;
		const auto key = makeIdentity(file, entry, type, recordSignature, &usedPluginStringID);
		if (usedPluginStringID)
		{
			++result.pluginStringIDIndexes;
		}

		SourceFreeTranslationData data;
		data.translationType = type;
		data.replacerText = TextNormalization::NormalizeUtf8(entry.dest);
		data.formID = key.formID;
		data.index = key.index;
		data.stringID = key.stringID;
		data.editorID = editorIdentity(entry);

		return TranslationCatalogRecord{
			.key = SourceFreeTranslationKeys::MakeKey(key),
			.data = std::move(data),
			.pluginName = SourceFreeTranslationKeys::NormalizePluginName(file.file.addon),
			.recordSignature = recordSignature
		};
	}
}

namespace TranslationCatalog
{
	TranslationCatalogBuildResult Build(std::span<const TranslationCatalogFile> files, XmlLoadOrder::Mode mode)
	{
		TranslationCatalogBuildResult result;

		std::vector<XmlLoadOrder::SortEntry> sortEntries;
		sortEntries.reserve(files.size());
		for (std::size_t i = 0; i < files.size(); ++i)
		{
			result.totalEntries += files[i].file.entries.size();
			sortEntries.push_back(XmlLoadOrder::SortEntry{
				.index = i,
				.file = fileSortName(files[i].file),
				.addon = files[i].file.addon,
				.pluginPriority = files[i].pluginPriority
			});
		}

		const auto order = XmlLoadOrder::SortIndices(sortEntries, mode);
		std::unordered_map<std::string, std::size_t> keyIndex;
		keyIndex.reserve(result.totalEntries);
		result.records.reserve(result.totalEntries);

		for (const auto sortedIndex : order)
		{
			const auto& input = files[sortEntries[sortedIndex].index];
			for (const auto& entry : input.file.entries)
			{
				if (isBlank(entry.dest))
				{
					++result.skippedEmptyDest;
					continue;
				}

				const auto recordSignature = XmlTranslationMapping::NormalizeSignature(entry.record);
				const auto type = XmlTranslationMapping::GetTranslationType(recordSignature);
				if (type == TranslationType::kUnknown)
				{
					++result.skippedUnknownType;
					continue;
				}

				SourceFreeTranslationKey identityProbe;
				identityProbe = makeIdentity(input, entry, type, recordSignature);
				if (!hasIdentity(identityProbe))
				{
					++result.skippedWithoutIdentity;
					continue;
				}

				auto record = makeRecord(input, entry, type, recordSignature, result);
				const auto [it, inserted] = keyIndex.emplace(record.key, result.records.size());
				if (inserted)
				{
					result.records.push_back(std::move(record));
					++result.acceptedEntries;
				}
				else if (mode == XmlLoadOrder::Mode::kFilename)
				{
					result.records[it->second] = std::move(record);
					++result.overwrittenEntries;
				}
			}
		}

		return result;
	}
}
