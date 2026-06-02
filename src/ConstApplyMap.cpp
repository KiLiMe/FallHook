// AI CONTEXT: Converts source-free catalog records into direct const-apply maps.
// Depends on ConstApplyMap declarations and source-free editor ID normalization.
// Runtime assumptions: version-neutral Fallout 4 direct const-apply translation targets.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: unsupported runtime/data-slot categories are skipped, not keyed by Source text.
#include "ConstApplyMap.h"

#include <utility>

namespace
{
	bool hasText(const SourceFreeTranslationData& data)
	{
		return !data.replacerText.empty();
	}

	ConstApplyEntry makeEntry(const TranslationCatalogRecord& record)
	{
		return ConstApplyEntry{
			.data = record.data,
			.pluginName = record.pluginName,
			.recordSignature = record.recordSignature
		};
	}

	bool isIndexedFullNameFragment(const TranslationCatalogRecord& record)
	{
		return record.data.translationType == TranslationType::kFullName &&
			record.data.index.has_value() &&
			(record.recordSignature == "WEAP FULL" || record.recordSignature == "ARMO FULL");
	}

	[[nodiscard]] bool isActorValueAnamRecord(const TranslationCatalogRecord& record)
	{
		return record.recordSignature == "AVIF ANAM";
	}
}

namespace ConstApplyMap
{
	bool IsDirectConstApplyType(TranslationType type) noexcept
	{
		switch (type)
		{
		case TranslationType::kFullName:
		case TranslationType::kLoadScreenDescription:
		case TranslationType::kMagicDescription:
		case TranslationType::kShortName:
		case TranslationType::kWordOfPower:
		case TranslationType::kGameSetting:
		case TranslationType::kRegion:
		case TranslationType::kQuestObjective:
		case TranslationType::kButtonText1:
		case TranslationType::kReference:
		case TranslationType::kBodyPartName:
		case TranslationType::kFactionMaleRank:
		case TranslationType::kFactionFemaleRank:
		case TranslationType::kTerminalResultText:
		case TranslationType::kTerminalBodyText:
		case TranslationType::kTerminalItemText:
		case TranslationType::kTerminalResponseText:
		case TranslationType::kTerminalHeaderText:
		case TranslationType::kTerminalWelcomeText:
		case TranslationType::kLocationFullName:
		case TranslationType::kAlchemyAddictionName:
		case TranslationType::kAmmoShortDescription:
		case TranslationType::kDoorAlternateOpenText:
		case TranslationType::kDoorAlternateCloseText:
		case TranslationType::kMessageShortName:
		case TranslationType::kActorValueAbbreviation:
			return true;
		default:
			return false;
		}
	}

	ConstApplyMaps Build(const TranslationCatalogBuildResult& catalog)
	{
		ConstApplyMaps maps;
		maps.stats.totalRecords = catalog.records.size();
		maps.formEntries.reserve(catalog.records.size());
		maps.editorIDFormEntries.reserve(catalog.records.size());
		maps.gameSettingEntries.reserve(catalog.records.size());

		for (const auto& record : catalog.records)
		{
			const auto& data = record.data;
			if (!hasText(data))
			{
				++maps.stats.skippedEmptyText;
				continue;
			}
			if (!IsDirectConstApplyType(data.translationType))
			{
				++maps.stats.skippedUnsupportedType;
				continue;
			}
			if (isIndexedFullNameFragment(record))
			{
				++maps.stats.skippedUnsupportedType;
				continue;
			}
			if (data.translationType == TranslationType::kActorValueAbbreviation &&
				!isActorValueAnamRecord(record))
			{
				++maps.stats.skippedUnsupportedType;
				continue;
			}
			if (data.translationType == TranslationType::kGameSetting)
			{
				if (!data.editorID || data.editorID->empty())
				{
					++maps.stats.skippedMissingTarget;
					continue;
				}

				const auto key = SourceFreeTranslationKeys::NormalizeEditorID(*data.editorID);
				const auto [_, inserted] = maps.gameSettingEntries.insert_or_assign(key, makeEntry(record));
				if (inserted)
				{
					++maps.stats.gameSettingEntries;
				}
				continue;
			}

			if (data.formID)
			{
				maps.formEntries.emplace(*data.formID, makeEntry(record));
				++maps.stats.formEntries;
				continue;
			}
			if (data.editorID && !data.editorID->empty())
			{
				maps.editorIDFormEntries.push_back(makeEntry(record));
				++maps.stats.editorIDFormEntries;
				continue;
			}

			++maps.stats.skippedMissingTarget;
		}

		return maps;
	}
}
