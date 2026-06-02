// AI CONTEXT: Builds cached per-runtime prepared catalog sections from the full source-free catalog.
// Depends on TranslationPreparedData declarations and const-apply type rules.
// Runtime assumptions: version-neutral Fallout 4 cached data grouping before runtime map construction.
// Version-specific logic: none; this module only classifies source-free records.
// Source-free policy: only record type/signature/identity are used; XML Source text is never read.
#include "TranslationPreparedData.h"

#include "ConstApplyMap.h"
#include "PluginEdidIndex.h"

#include <string_view>

using namespace std::literals;

namespace
{
	[[nodiscard]] bool hasText(const TranslationCatalogRecord& record)
	{
		return !record.data.replacerText.empty();
	}

	[[nodiscard]] bool isIndexedItemFullName(const TranslationCatalogRecord& record)
	{
		return record.data.translationType == TranslationType::kFullName &&
			record.data.index.has_value() &&
			(record.recordSignature == "WEAP FULL" || record.recordSignature == "ARMO FULL");
	}

	[[nodiscard]] bool isVerifiedTemplateFullName(const TranslationCatalogRecord& record)
	{
		std::uint32_t slot = 0;
		return isIndexedItemFullName(record) &&
			PluginEdidIndex::DecodeTemplateFullNameSlot(*record.data.index, slot);
	}

	[[nodiscard]] bool isDirectConstApply(const TranslationCatalogRecord& record)
	{
		if (!hasText(record) || !ConstApplyMap::IsDirectConstApplyType(record.data.translationType))
		{
			return false;
		}
		if (isIndexedItemFullName(record))
		{
			return false;
		}
		if (record.data.translationType == TranslationType::kActorValueAbbreviation)
		{
			return record.recordSignature == "AVIF ANAM";
		}
		return true;
	}

	[[nodiscard]] bool isQuestJournal(const TranslationCatalogRecord& record)
	{
		return hasText(record) &&
			record.data.translationType == TranslationType::kRuntimeLegacy &&
			record.recordSignature == "QUST CNAM";
	}

	[[nodiscard]] bool isPipboyLog(const TranslationCatalogRecord& record)
	{
		return hasText(record) &&
			record.data.translationType == TranslationType::kGameSetting &&
			record.recordSignature == "GMST DATA";
	}

	[[nodiscard]] bool isDescription(const TranslationCatalogRecord& record)
	{
		return hasText(record) &&
			record.data.translationType == TranslationType::kRuntime1 &&
			record.recordSignature.ends_with(" DESC"sv);
	}

	[[nodiscard]] bool isDialogue(const TranslationCatalogRecord& record)
	{
		if (!hasText(record))
		{
			return false;
		}
		return (record.recordSignature == "INFO NAM1" &&
				   record.data.translationType == TranslationType::kRuntimeIndex) ||
			(record.recordSignature == "INFO RNAM" &&
				record.data.translationType == TranslationType::kRuntime2) ||
			(record.recordSignature == "DIAL FULL" &&
				record.data.translationType == TranslationType::kRuntime1);
	}

	[[nodiscard]] bool isActivationText(const TranslationCatalogRecord& record)
	{
		return hasText(record) && record.data.translationType == TranslationType::kActivationText;
	}

	[[nodiscard]] bool isPerkActivateChoice(const TranslationCatalogRecord& record)
	{
		return hasText(record) &&
			record.data.translationType == TranslationType::kButtonText2 &&
			record.recordSignature == "PERK EPF2";
	}

	[[nodiscard]] bool isFullNameLoad(const TranslationCatalogRecord& record)
	{
		if (!hasText(record) || !record.recordSignature.ends_with(" FULL"sv))
		{
			return false;
		}
		if (isVerifiedTemplateFullName(record))
		{
			return false;
		}
		switch (record.data.translationType)
		{
		case TranslationType::kFullName:
		case TranslationType::kNpcFullName:
		case TranslationType::kLocationFullName:
			return true;
		default:
			return false;
		}
	}

	[[nodiscard]] bool isInventoryTemplateName(const TranslationCatalogRecord& record)
	{
		return hasText(record) &&
			isVerifiedTemplateFullName(record);
	}

	[[nodiscard]] bool isInstanceNaming(const TranslationCatalogRecord& record)
	{
		return hasText(record) &&
			record.data.translationType == TranslationType::kInstanceNamePart &&
			record.recordSignature == "INNR WNAM";
	}

	using Filter = bool (*)(const TranslationCatalogRecord&);

	[[nodiscard]] TranslationCatalogBuildResult makeSection(
		const TranslationCatalogBuildResult& catalog,
		Filter filter)
	{
		TranslationCatalogBuildResult section;
		section.records.reserve(catalog.records.size());
		for (const auto& record : catalog.records)
		{
			if (filter(record))
			{
				section.records.push_back(record);
			}
		}
		section.totalEntries = section.records.size();
		section.acceptedEntries = section.records.size();
		return section;
	}
}

namespace TranslationPreparedDataBuilder
{
	TranslationPreparedData Build(const TranslationCatalogBuildResult& catalog)
	{
		return TranslationPreparedData{
			.constApply = makeSection(catalog, isDirectConstApply),
			.questJournal = makeSection(catalog, isQuestJournal),
			.pipboyLog = makeSection(catalog, isPipboyLog),
			.description = makeSection(catalog, isDescription),
			.dialogue = makeSection(catalog, isDialogue),
			.activationText = makeSection(catalog, isActivationText),
			.perkActivateChoice = makeSection(catalog, isPerkActivateChoice),
			.fullNameLoad = makeSection(catalog, isFullNameLoad),
			.inventoryTemplateNames = makeSection(catalog, isInventoryTemplateName),
			.instanceNaming = makeSection(catalog, isInstanceNaming)
		};
	}
}
