// AI CONTEXT: Builds source-free FULL maps used while TESFullName chunks are loaded.
// Depends on RuntimeFormResolver for load-order-aware FormID calculation.
// Runtime scope is Fallout 4 1.11.191 load-time TESFullName destination assignment.
// Version-specific logic: none; runtime hook address stays in RuntimeFullNameLoadHook.
// Source-free policy: records are keyed by FormID/editorID/sID and never by XML Source or live text.
#include "PCH.h"

#include "RuntimeApplySettings.h"
#include "111191/RuntimeFormResolver.h"
#include "111191/RuntimeFullNameLoadTranslations.h"
#include "SourceFreeTranslationKey.h"

#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace Runtime111191
{
namespace
{
	std::mutex g_lock;
	const TranslationCatalogBuildResult* g_rebuiltCatalog{ nullptr };
	std::unordered_map<std::uint32_t, std::string> g_byFormID;
	std::unordered_map<std::uint32_t, RuntimeFullNameLoadTranslations::IndexedFallback> g_byNpcIndexedFallback;
	std::unordered_map<std::uint64_t, std::string> g_byFormStringID;
	std::unordered_map<std::string, std::string> g_byEditorID;
	std::unordered_set<std::uint32_t> g_stringIDs;
	RuntimeFullNameLoadTranslations::RebuildStats g_lastStats;
	bool g_needsOwnerWithoutStringID{ false };
	bool g_hasOwnerFallbackLookup{ false };

	[[nodiscard]] bool isLoadFullNameType(const TranslationCatalogRecord& record)
	{
		switch (record.data.translationType)
		{
		case TranslationType::kFullName:
		case TranslationType::kNpcFullName:
		case TranslationType::kLocationFullName:
			return record.recordSignature.ends_with(" FULL"sv);
		default:
			return false;
		}
	}

	[[nodiscard]] bool isHookFullNameSignature(const TranslationCatalogRecord& record)
	{
		return record.recordSignature == "WEAP FULL" ||
			record.recordSignature == "ARMO FULL" ||
			record.recordSignature == "OMOD FULL" ||
			record.recordSignature == "MISC FULL" ||
			record.recordSignature == "NPC_ FULL";
	}

	[[nodiscard]] bool isNpcFullNameRecord(const TranslationCatalogRecord& record)
	{
		return record.data.translationType == TranslationType::kNpcFullName &&
			record.recordSignature == "NPC_ FULL";
	}

	[[nodiscard]] std::string editorKey(std::string_view editorID)
	{
		return SourceFreeTranslationKeys::NormalizeEditorID(editorID);
	}

	[[nodiscard]] std::uint64_t formStringKey(std::uint32_t formID, std::uint32_t stringID) noexcept
	{
		return (static_cast<std::uint64_t>(formID) << 32) | stringID;
	}

	[[nodiscard]] std::optional<std::uint32_t> resolveRuntimeFormID(const TranslationCatalogRecord& record)
	{
		if (record.data.formID)
		{
			return RuntimeFormResolver::ResolveRawFormID(*record.data.formID, record.pluginName);
		}
		if (auto* form = RuntimeFormResolver::ResolveEditorForm(record.data))
		{
			return form->formID;
		}
		return std::nullopt;
	}
}

namespace RuntimeFullNameLoadTranslations
{
	RebuildStats Rebuild(const TranslationCatalogBuildResult& catalog, bool force)
	{
		std::scoped_lock lock{ g_lock };
		if (!force && g_rebuiltCatalog == std::addressof(catalog))
		{
			return g_lastStats;
		}

		g_byFormID.clear();
		g_byNpcIndexedFallback.clear();
		g_byFormStringID.clear();
		g_byEditorID.clear();
		g_stringIDs.clear();
		g_needsOwnerWithoutStringID = false;
		g_hasOwnerFallbackLookup = false;
		RebuildStats stats;

		for (const auto& record : catalog.records)
		{
			if (!isLoadFullNameType(record))
			{
				++stats.skippedWrongType;
				continue;
			}
			if (!isHookFullNameSignature(record))
			{
				++stats.skippedUnsupportedSignature;
				continue;
			}
			if (record.data.replacerText.empty())
			{
				++stats.skippedEmptyText;
				continue;
			}
			bool accepted = false;
			const auto runtimeFormID = resolveRuntimeFormID(record);
			if (record.data.formID && !runtimeFormID)
			{
				++stats.skippedUnresolvedFormID;
			}
			if (runtimeFormID && record.data.stringID)
			{
				g_stringIDs.insert(*record.data.stringID);
				g_byFormStringID.insert_or_assign(
					formStringKey(*runtimeFormID, *record.data.stringID),
					record.data.replacerText);
				++stats.sidEntries;
				accepted = true;
			}

			if (runtimeFormID && record.data.index.has_value() && isNpcFullNameRecord(record))
			{
				auto& fallback = g_byNpcIndexedFallback[*runtimeFormID];
				if (fallback.text.empty() || *record.data.index >= fallback.index)
				{
					fallback.index = *record.data.index;
					fallback.text = record.data.replacerText;
				}
				++stats.indexedBaseEntries;
				accepted = true;
				g_needsOwnerWithoutStringID = g_needsOwnerWithoutStringID || !record.data.stringID;
			}

			if (runtimeFormID && record.data.index == 0 && !isNpcFullNameRecord(record))
			{
				g_byFormID.insert_or_assign(*runtimeFormID, record.data.replacerText);
				++stats.indexedBaseEntries;
				accepted = true;
				g_needsOwnerWithoutStringID = g_needsOwnerWithoutStringID || !record.data.stringID;
			}

			if (record.data.index.has_value())
			{
				if (!accepted)
				{
					++stats.skippedIndexed;
				}
				stats.accepted += accepted ? 1 : 0;
				continue;
			}

			if (runtimeFormID)
			{
				g_byFormID.insert_or_assign(*runtimeFormID, record.data.replacerText);
				++stats.formEntries;
				accepted = true;
				g_needsOwnerWithoutStringID = g_needsOwnerWithoutStringID || !record.data.stringID;
			}
			if (!accepted && record.data.editorID && !record.data.editorID->empty())
			{
				g_byEditorID.insert_or_assign(editorKey(*record.data.editorID), record.data.replacerText);
				++stats.editorEntries;
				accepted = true;
				g_needsOwnerWithoutStringID = g_needsOwnerWithoutStringID || !record.data.stringID;
			}

			if (accepted)
			{
				++stats.accepted;
			}
			else
			{
				++stats.skippedMissingTarget;
			}
		}

		g_hasOwnerFallbackLookup = g_needsOwnerWithoutStringID ||
			!g_byFormID.empty() ||
			!g_byEditorID.empty() ||
			!g_byNpcIndexedFallback.empty();
		g_rebuiltCatalog = std::addressof(catalog);
		g_lastStats = stats;
		if (RuntimeApplySettings::Load().TraceEnabled())
		{
			REX::INFO(
				"{} full-name-load map built: accepted={} formEntries={} editorEntries={} sidEntries={} indexedBaseEntries={} indexedNoIdentity={} unsupportedSignature={} unresolvedFormID={} emptyText={}.",
				Plugin::NAME,
				stats.accepted,
				stats.formEntries,
				stats.editorEntries,
				stats.sidEntries,
				stats.indexedBaseEntries,
				stats.skippedIndexed,
				stats.skippedUnsupportedSignature,
				stats.skippedUnresolvedFormID,
				stats.skippedEmptyText);
		}
		return stats;
	}

	bool MayNeedOwnerLookup(std::optional<std::uint32_t> stringID) noexcept
	{
		if (stringID && g_stringIDs.contains(*stringID))
		{
			return true;
		}
		return g_hasOwnerFallbackLookup;
	}

	LookupResult Lookup(const RE::TESForm* form, std::optional<std::uint32_t> stringID)
	{
		if (!form)
		{
			return {};
		}

		if (stringID)
		{
			if (const auto it = g_byFormStringID.find(formStringKey(form->formID, *stringID)); it != g_byFormStringID.end())
			{
				return LookupResult{
					.text = std::addressof(it->second),
					.source = LookupSource::kStringID
				};
			}
		}

		if (const auto it = g_byFormID.find(form->formID); it != g_byFormID.end())
		{
			return LookupResult{
				.text = std::addressof(it->second),
				.source = LookupSource::kFormID
			};
		}

		if (!g_byEditorID.empty())
		{
			const auto* editorID = form->GetFormEditorID();
			if (editorID && editorID[0] != '\0')
			{
				if (const auto it = g_byEditorID.find(editorKey(editorID)); it != g_byEditorID.end())
				{
					return LookupResult{
						.text = std::addressof(it->second),
						.source = LookupSource::kEditorID
					};
				}
			}
		}

		if (form->GetFormType() == RE::ENUM_FORM_ID::kNPC_)
		{
			if (const auto it = g_byNpcIndexedFallback.find(form->formID); it != g_byNpcIndexedFallback.end())
			{
				return LookupResult{
					.text = std::addressof(it->second.text),
					.source = LookupSource::kNpcIndexedFallback,
					.index = it->second.index
				};
			}
		}

		return {};
	}
}

} // namespace Runtime111191
 
