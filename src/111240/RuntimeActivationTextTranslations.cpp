// AI CONTEXT: Builds source-free activation prompt maps for HUDRollover action patching.
// Depends on RuntimeFormResolver, SourceFreeTranslationKey, and activation translation declarations.
// Runtime scope is Fallout 4 1.11.240 ACTI/FLOR/FURN/NPC_ activation prompt records.
// Version-specific logic: 1.11.240 HUDRollover action lookup data only.
// Source-free policy: stores destination text keyed by form/sID/editor identity; Source is ignored.
#include "PCH.h"

#include "111240/RuntimeActivationTextTranslations.h"

#include "RuntimeApplySettings.h"
#include "111240/RuntimeFormResolver.h"
#include "SourceFreeTranslationKey.h"

#include "RE/T/TESForm.h"

#include <atomic>
#include <mutex>
#include <string_view>
#include <unordered_map>

namespace Runtime111240
{
namespace
{
	std::mutex g_lock;
	const TranslationCatalogBuildResult* g_rebuiltCatalog{ nullptr };
	std::unordered_map<std::uint32_t, std::string> g_byFormID;
	std::unordered_map<std::uint64_t, std::string> g_byFormStringID;
	std::unordered_map<std::string, std::string> g_byEditorID;
	std::optional<std::string> g_knownFurnitureUseText;
	RuntimeActivationTextTranslations::RebuildStats g_lastStats;
	std::atomic_uint32_t g_lookupTraceLines{ 0 };
	constexpr std::uint32_t kLookupTraceLimit{ 384 };
	constexpr std::uint32_t kWorkshopArtilleryUseSid{ 0x017FE9 };
	constexpr std::uint32_t kWorkshopGuardPostUseSid{ 0x02B63B };

	[[nodiscard]] bool isActivationRecord(const TranslationCatalogRecord& record)
	{
		return record.data.translationType == TranslationType::kActivationText;
	}

	[[nodiscard]] bool isKnownFurnitureUseRecord(const TranslationCatalogRecord& record)
	{
		if (record.recordSignature != "FURN ATTX" || !record.data.stringID)
		{
			return false;
		}
		return *record.data.stringID == kWorkshopArtilleryUseSid ||
			*record.data.stringID == kWorkshopGuardPostUseSid;
	}

	[[nodiscard]] std::string editorKey(std::string_view editorID)
	{
		return SourceFreeTranslationKeys::NormalizeEditorID(editorID);
	}

	void traceActivationLookup(
		std::string_view stage,
		const RE::TESForm* form,
		std::optional<std::uint32_t> stringID,
		std::size_t textLen = 0)
	{
		const auto settings = RuntimeApplySettings::Load();
		if (!settings.TraceEnabled() ||
			g_lookupTraceLines.fetch_add(1, std::memory_order_relaxed) >= kLookupTraceLimit)
		{
			return;
		}

		const auto* editor = form ? form->GetFormEditorID() : nullptr;
		REX::INFO(
			"{} activation-lookup trace stage={} form={:08X} formType={} editor={} sid={} textLen={}",
			Plugin::NAME,
			stage,
			form ? form->formID : 0,
			form ? form->GetFormTypeString() : "",
			editor ? editor : "",
			stringID.value_or(0xFFFFFFFFu),
			textLen);
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

namespace RuntimeActivationTextTranslations
{
	RebuildStats Rebuild(const TranslationCatalogBuildResult& catalog, bool force)
	{
		std::scoped_lock lock{ g_lock };
		if (!force && g_rebuiltCatalog == std::addressof(catalog))
		{
			return g_lastStats;
		}

		g_byFormID.clear();
		g_byFormStringID.clear();
		g_byEditorID.clear();
		g_knownFurnitureUseText.reset();
		g_lookupTraceLines.store(0, std::memory_order_relaxed);
		RebuildStats stats;

		for (const auto& record : catalog.records)
		{
			if (!isActivationRecord(record))
			{
				++stats.skippedWrongType;
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
				g_byFormStringID.insert_or_assign(
					formStringKey(*runtimeFormID, *record.data.stringID),
					record.data.replacerText);
				++stats.sidEntries;
				accepted = true;
			}
			if (runtimeFormID)
			{
				g_byFormID.insert_or_assign(*runtimeFormID, record.data.replacerText);
				++stats.formEntries;
				accepted = true;
			}
			if (!accepted && record.data.editorID && !record.data.editorID->empty())
			{
				g_byEditorID.insert_or_assign(editorKey(*record.data.editorID), record.data.replacerText);
				++stats.editorEntries;
				accepted = true;
			}

			if (accepted)
			{
				++stats.accepted;
				if (isKnownFurnitureUseRecord(record))
				{
					g_knownFurnitureUseText = record.data.replacerText;
					++stats.knownFurnitureUseEntries;
				}
			}
			else
			{
				++stats.skippedMissingTarget;
			}
		}

		g_rebuiltCatalog = std::addressof(catalog);
		g_lastStats = stats;
		if (RuntimeApplySettings::Load().TraceEnabled())
		{
			REX::INFO(
				"{} activation-text map built: accepted={} formEntries={} editorEntries={} sidEntries={} furnitureUse={} unresolvedFormID={} emptyText={}.",
				Plugin::NAME,
				stats.accepted,
				stats.formEntries,
				stats.editorEntries,
				stats.sidEntries,
				stats.knownFurnitureUseEntries,
				stats.skippedUnresolvedFormID,
				stats.skippedEmptyText);
		}
		return stats;
	}

	std::optional<std::string> Lookup(const RE::TESForm* form, std::optional<std::uint32_t> stringID)
	{
		if (!form)
		{
			traceActivationLookup("miss-null-form", form, stringID);
			return std::nullopt;
		}

		std::scoped_lock lock{ g_lock };
		if (stringID)
		{
			const auto bySid = g_byFormStringID.find(formStringKey(form->formID, *stringID));
			if (bySid != g_byFormStringID.end())
			{
				traceActivationLookup("hit-form-sid", form, stringID, bySid->second.size());
				return bySid->second;
			}
		}
		if (const auto byForm = g_byFormID.find(form->formID); byForm != g_byFormID.end())
		{
			traceActivationLookup("hit-form", form, stringID, byForm->second.size());
			return byForm->second;
		}

		const auto* editorID = form->GetFormEditorID();
		if (!editorID || editorID[0] == '\0')
		{
			traceActivationLookup("miss-no-editor", form, stringID);
			return std::nullopt;
		}

		const auto byEditor = g_byEditorID.find(editorKey(editorID));
		if (byEditor != g_byEditorID.end())
		{
			traceActivationLookup("hit-editor", form, stringID, byEditor->second.size());
			return byEditor->second;
		}

		traceActivationLookup("miss", form, stringID);
		return std::nullopt;
	}

	std::optional<std::string> KnownFurnitureUseText()
	{
		std::scoped_lock lock{ g_lock };
		return g_knownFurnitureUseText;
	}
}

} // namespace Runtime111240
