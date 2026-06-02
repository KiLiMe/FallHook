// AI CONTEXT: Builds and queries source-free TESDescription translation maps.
// Depends on RuntimeFormResolver for load-order-aware form resolution and RuntimeApplySettings for diagnostics.
// Runtime scope is Fallout 4 1.11.191 loaded form data for DESC records.
// Version-specific logic: Fallout 4 1.11.191 only; no alternate runtime branches.
// Source-free policy: accepts catalog form/editor IDs and stores destination text only; Source text is never read.
#include "PCH.h"

#include "RuntimeApplySettings.h"
#include "111191/RuntimeDescriptionTranslations.h"
#include "111191/RuntimeFormResolver.h"

#include <mutex>
#include <unordered_map>

namespace Runtime111191
{
namespace
{
	std::mutex g_lock;
	std::unordered_map<std::uint32_t, std::string> g_byFormID;
	std::unordered_map<const RE::TESDescription*, std::uint32_t> g_ownerByDescription;

	struct BuildStats
	{
		std::size_t accepted{ 0 };
		std::size_t skippedWrongType{ 0 };
		std::size_t skippedMissingForm{ 0 };
		std::size_t skippedNoDescription{ 0 };
		std::size_t bookItemCardOwners{ 0 };
	};

	[[nodiscard]] bool isDescriptionRecord(const TranslationCatalogRecord& record)
	{
		return record.data.translationType == TranslationType::kRuntime1 &&
			record.recordSignature.ends_with(" DESC"sv);
	}

	[[nodiscard]] RE::TESForm* resolveForm(const TranslationCatalogRecord& record)
	{
		if (record.data.formID)
		{
			return RuntimeFormResolver::ResolveRawForm(*record.data.formID, record.pluginName);
		}
		return RuntimeFormResolver::ResolveEditorForm(record.data);
	}

	void addDescriptionOwner(RE::TESForm* form, std::string_view text, BuildStats& stats)
	{
		if (!form)
		{
			++stats.skippedMissingForm;
			return;
		}

		const auto* description = form->As<RE::TESDescription>();
		if (!description)
		{
			++stats.skippedNoDescription;
			return;
		}

		g_byFormID.insert_or_assign(form->formID, std::string{ text });
		g_ownerByDescription.insert_or_assign(description, form->formID);
		if (const auto* book = form->As<RE::TESObjectBOOK>())
		{
			g_ownerByDescription.insert_or_assign(std::addressof(book->itemCardDescription), form->formID);
			++stats.bookItemCardOwners;
		}
		++stats.accepted;
	}

	[[nodiscard]] std::optional<RuntimeDescriptionTranslations::LookupResult> lookupLocked(
		const RE::TESDescription* description,
		const RE::TESForm* form)
	{
		if (form)
		{
			const auto it = g_byFormID.find(form->formID);
			if (it != g_byFormID.end())
			{
				return RuntimeDescriptionTranslations::LookupResult{
					.formID = form->formID,
					.text = it->second,
					.usedOwnerIndex = false
				};
			}
		}

		const auto owner = g_ownerByDescription.find(description);
		if (owner == g_ownerByDescription.end())
		{
			return std::nullopt;
		}

		const auto it = g_byFormID.find(owner->second);
		if (it == g_byFormID.end())
		{
			return std::nullopt;
		}

		return RuntimeDescriptionTranslations::LookupResult{
			.formID = owner->second,
			.text = it->second,
			.usedOwnerIndex = true
		};
	}
}

namespace RuntimeDescriptionTranslations
{
	void Rebuild(const TranslationCatalogBuildResult& catalog)
	{
		std::scoped_lock lock{ g_lock };
		g_byFormID.clear();
		g_ownerByDescription.clear();

		BuildStats stats;
		for (const auto& record : catalog.records)
		{
			if (!isDescriptionRecord(record))
			{
				++stats.skippedWrongType;
				continue;
			}
			addDescriptionOwner(resolveForm(record), record.data.replacerText, stats);
		}

		if (RuntimeApplySettings::Load().TraceEnabled())
		{
			REX::INFO(
				"{} description map built: accepted={} skippedMissingForm={} skippedNoDescription={} bookItemCardOwners={}.",
				Plugin::NAME,
				stats.accepted,
				stats.skippedMissingForm,
				stats.skippedNoDescription,
				stats.bookItemCardOwners);
		}
	}

	std::optional<LookupResult> Lookup(const RE::TESDescription* description, const RE::TESForm* form)
	{
		if (!description && !form)
		{
			return std::nullopt;
		}

		std::scoped_lock lock{ g_lock };
		return lookupLocked(description, form);
	}
}

} // namespace Runtime111191
