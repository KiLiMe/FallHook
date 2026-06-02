// AI CONTEXT: Builds and applies indexed WEAP/ARMO FULL translations to loaded template item names.
// Depends on RuntimeFormResolver, RuntimeTextStringAssign, and CommonLibF4 templates.
// Runtime scope is Fallout 4 1.10.163 TESBoundObject objectTemplate item names only.
// Version-specific logic: none; this module is catalog-wide data mutation without a hot hook.
// Source-free policy: applies by form/editor identity plus REC index/sID; never matches Source text.
#include "PCH.h"

#include "PluginEdidIndex.h"
#include "RuntimeApplySettings.h"
#include "110163/RuntimeFormResolver.h"
#include "110163/RuntimeInventoryTemplateNames.h"
#include "110163/RuntimeLocalizedStringID.h"
#include "110163/RuntimeTextStringAssign.h"
#include "SourceFreeTranslationKey.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace
{
	enum class ApplyItemStatus : std::uint8_t
	{
		kApplied,
		kNoTranslation,
		kStringIDMismatch
	};

	struct TemplateTranslation
	{
		std::uint32_t index{ 0 };
		std::optional<std::uint32_t> stringID;
		std::string text;
	};

	struct EditorTemplateTranslations
	{
		std::string editorID;
		std::vector<TemplateTranslation> entries;
	};

	std::mutex g_lock;
	const TranslationCatalogBuildResult* g_rebuiltCatalog{ nullptr };
	std::unordered_map<std::uint32_t, std::vector<TemplateTranslation>> g_byFormID;
	std::unordered_map<std::string, EditorTemplateTranslations> g_byEditorID;
	RuntimeInventoryTemplateNames::ApplyStats g_buildStats;
	RuntimeInventoryTemplateNames::ApplyStats g_lastStats;
	const TranslationCatalogBuildResult* g_appliedCatalog{ nullptr };
	std::atomic_bool g_ready{ false };

	[[nodiscard]] bool isTemplateFullNameRecord(const TranslationCatalogRecord& record)
	{
		std::uint32_t slot = 0;
		return record.data.translationType == TranslationType::kFullName &&
			record.data.index.has_value() &&
			PluginEdidIndex::DecodeTemplateFullNameSlot(*record.data.index, slot) &&
			(record.recordSignature == "WEAP FULL" || record.recordSignature == "ARMO FULL");
	}

	[[nodiscard]] std::uint32_t templateSlot(const TranslationCatalogRecord& record)
	{
		std::uint32_t slot = 0;
		(void)PluginEdidIndex::DecodeTemplateFullNameSlot(*record.data.index, slot);
		return slot;
	}

	[[nodiscard]] std::optional<std::uint32_t> resolveRuntimeFormID(const TranslationCatalogRecord& record)
	{
		if (record.data.formID)
		{
			return RuntimeFormResolver::ResolveRawFormID(*record.data.formID, record.pluginName);
		}
		return std::nullopt;
	}

	[[nodiscard]] std::string editorKey(std::string_view editorID)
	{
		return SourceFreeTranslationKeys::NormalizeEditorID(editorID);
	}

	[[nodiscard]] bool isWeaponOrArmor(const RE::TESForm* form)
	{
		return form && form->Is(RE::ENUM_FORM_ID::kWEAP, RE::ENUM_FORM_ID::kARMO);
	}

	[[nodiscard]] RE::BGSMod::Template::Item* templateItemAt(RE::TESBoundObject* object, std::uint32_t index)
	{
		if (!object || index >= object->objectTemplate.items.size())
		{
			return nullptr;
		}

		return object->objectTemplate.items[index];
	}

	[[nodiscard]] bool stringIDMatches(const TemplateTranslation& translation, const RE::BGSMod::Template::Item& item)
	{
		if (!translation.stringID)
		{
			return true;
		}
		const auto runtimeStringID = RuntimeLocalizedStringID::Read(item.fullName);
		return !runtimeStringID || *runtimeStringID == *translation.stringID;
	}

	[[nodiscard]] std::optional<std::uint32_t> readStringID(const RE::BGSMod::Template::Item& item)
	{
		return RuntimeLocalizedStringID::Read(item.fullName);
	}

	ApplyItemStatus applyItemTranslationsLocked(
		RE::BGSMod::Template::Item& item,
		std::uint32_t index,
		const std::vector<TemplateTranslation>& translations)
	{
		bool sawStringIDMismatch = false;
		for (const auto& translation : translations)
		{
			if (translation.index != index)
			{
				continue;
			}
			if (!stringIDMatches(translation, item))
			{
				sawStringIDMismatch = true;
				continue;
			}

			if (std::string_view{ item.fullName } != translation.text)
			{
				RuntimeTextStringAssign::AssignPlainLocalized(item.fullName, translation.text);
			}
			return ApplyItemStatus::kApplied;
		}

		const auto runtimeStringID = readStringID(item);
		if (runtimeStringID)
		{
			for (const auto& translation : translations)
			{
				if (!translation.stringID || *translation.stringID != *runtimeStringID)
				{
					continue;
				}

				if (std::string_view{ item.fullName } != translation.text)
				{
					RuntimeTextStringAssign::AssignPlainLocalized(item.fullName, translation.text);
				}
				return ApplyItemStatus::kApplied;
			}
		}
		return sawStringIDMismatch ? ApplyItemStatus::kStringIDMismatch : ApplyItemStatus::kNoTranslation;
	}

	void applyTranslationsToObject(
		RE::TESBoundObject& object,
		const std::vector<TemplateTranslation>& translations,
		RuntimeInventoryTemplateNames::ApplyStats& stats)
	{
		const auto count = object.objectTemplate.items.size();
		for (std::uint32_t index = 0; index < count; ++index)
		{
			auto* item = templateItemAt(std::addressof(object), index);
			if (!item)
			{
				++stats.skippedMissingTemplateItem;
				continue;
			}

			const auto status = applyItemTranslationsLocked(*item, index, translations);
			if (status == ApplyItemStatus::kApplied)
			{
				++stats.applied;
			}
			else if (status == ApplyItemStatus::kStringIDMismatch)
			{
				++stats.skippedStringIDMismatch;
			}
		}

		for (const auto& translation : translations)
		{
			if (translation.index >= count)
			{
				++stats.skippedMissingTemplateItem;
			}
		}
	}

	RuntimeInventoryTemplateNames::ApplyStats applyAllLocked()
	{
		RuntimeInventoryTemplateNames::ApplyStats stats;
		for (const auto& [formID, translations] : g_byFormID)
		{
			auto* form = RE::TESForm::GetFormByID(formID);
			if (!form)
			{
				stats.skippedMissingForm += translations.size();
				continue;
			}
			auto* object = form->As<RE::TESBoundObject>();
			if (!object || !isWeaponOrArmor(object))
			{
				stats.skippedWrongFormType += translations.size();
				continue;
			}

			applyTranslationsToObject(*object, translations, stats);
		}

		for (const auto& [_, entry] : g_byEditorID)
		{
			auto* form = RE::TESForm::GetFormByEditorID(RE::BSFixedString{ entry.editorID.c_str() });
			if (!form)
			{
				stats.skippedMissingForm += entry.entries.size();
				continue;
			}
			auto* object = form->As<RE::TESBoundObject>();
			if (!object || !isWeaponOrArmor(object))
			{
				stats.skippedWrongFormType += entry.entries.size();
				continue;
			}

			applyTranslationsToObject(*object, entry.entries, stats);
		}
		return stats;
	}
}

namespace RuntimeInventoryTemplateNames
{
	ApplyStats Rebuild(const TranslationCatalogBuildResult& catalog)
	{
		std::scoped_lock lock{ g_lock };
		if (g_rebuiltCatalog == std::addressof(catalog) && g_ready.load(std::memory_order_acquire))
		{
			return g_lastStats;
		}

		g_byFormID.clear();
		g_byEditorID.clear();
		g_appliedCatalog = nullptr;
		ApplyStats stats;
		for (const auto& record : catalog.records)
		{
			if (!isTemplateFullNameRecord(record))
			{
				++stats.skippedWrongType;
				continue;
			}
			if (record.data.replacerText.empty())
			{
				++stats.skippedEmptyText;
				continue;
			}

			const TemplateTranslation translation{
				.index = templateSlot(record),
				.stringID = record.data.stringID,
				.text = record.data.replacerText
			};
			if (const auto runtimeFormID = resolveRuntimeFormID(record))
			{
				g_byFormID[*runtimeFormID].push_back(translation);
				continue;
			}
			if (record.data.editorID && !record.data.editorID->empty())
			{
				auto& entry = g_byEditorID[editorKey(*record.data.editorID)];
				entry.editorID = *record.data.editorID;
				entry.entries.push_back(translation);
				continue;
			}
			if (record.data.formID)
			{
				++stats.skippedMissingForm;
				continue;
			}
			++stats.skippedMissingForm;
		}

		g_rebuiltCatalog = std::addressof(catalog);
		g_ready.store(true, std::memory_order_release);
		g_buildStats = stats;
		g_lastStats = stats;
		return stats;
	}

	ApplyStats Apply(const TranslationCatalogBuildResult& catalog, bool force)
	{
		(void)Rebuild(catalog);

		ApplyStats stats;
		{
			std::scoped_lock lock{ g_lock };
			if (g_appliedCatalog == std::addressof(catalog))
			{
				stats = g_lastStats;
				if (!force)
				{
					stats.applied = 0;
					stats.skippedMissingTemplateItem = 0;
					stats.skippedStringIDMismatch = 0;
					if (RuntimeApplySettings::Load().TraceEnabled())
					{
						REX::INFO(
							"{} template-name apply cached: missingForm={} wrongFormType={} emptyText={}.",
							Plugin::NAME,
							stats.skippedMissingForm,
							stats.skippedWrongFormType,
							stats.skippedEmptyText);
					}
					return stats;
				}
			}

			stats = g_buildStats;
			const auto applyStats = applyAllLocked();
			stats.applied = applyStats.applied;
			stats.skippedMissingForm += applyStats.skippedMissingForm;
			stats.skippedWrongFormType += applyStats.skippedWrongFormType;
			stats.skippedMissingTemplateItem += applyStats.skippedMissingTemplateItem;
			stats.skippedStringIDMismatch += applyStats.skippedStringIDMismatch;
			g_appliedCatalog = std::addressof(catalog);
			g_lastStats = stats;
		}

		if (RuntimeApplySettings::Load().TraceEnabled())
		{
			REX::INFO(
				"{} template-name apply complete: applied={} missingForm={} wrongFormType={} missingTemplateItem={} stringIDMismatch={} emptyText={}.",
				Plugin::NAME,
				stats.applied,
				stats.skippedMissingForm,
				stats.skippedWrongFormType,
				stats.skippedMissingTemplateItem,
				stats.skippedStringIDMismatch,
				stats.skippedEmptyText);
		}
		return stats;
	}
}
