// AI CONTEXT: Builds source-free activation prompt lookup maps for runtime data interception.
// Depends on RuntimeFormResolver, SourceFreeTranslationKey, and activation translation declarations.
// Runtime scope is Fallout 4 1.10.163 activation override data records.
// Version-specific logic: none; runtime-specific hook installation is isolated elsewhere.
// Source-free policy: stores destination text keyed by resolved form/editor identity; Source is ignored.
#include "PCH.h"

#include "110163/RuntimeActivationTextTranslations.h"
#include "RuntimeApplySettings.h"
#include "110163/RuntimeFormResolver.h"
#include "SourceFreeTranslationKey.h"

#include "RE/T/TESForm.h"

#include <mutex>
#include <unordered_map>

namespace
{
	std::mutex g_lock;
	const TranslationCatalogBuildResult* g_rebuiltCatalog{ nullptr };
	std::unordered_map<std::uint32_t, std::string> g_byFormID;
	std::unordered_map<std::string, std::string> g_byEditorID;
	std::optional<std::string> g_knownFurnitureUseText;
	RuntimeActivationTextTranslations::RebuildStats g_lastStats;
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
		g_byEditorID.clear();
		g_knownFurnitureUseText.reset();
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
				"{} activation-text map built: accepted={} formEntries={} editorEntries={} furnitureUse={} unresolvedFormID={} emptyText={}.",
				Plugin::NAME,
				stats.accepted,
				stats.formEntries,
				stats.editorEntries,
				stats.knownFurnitureUseEntries,
				stats.skippedUnresolvedFormID,
				stats.skippedEmptyText);
		}
		return stats;
	}

	std::optional<std::string> Lookup(const RE::TESForm* form)
	{
		if (!form)
		{
			return std::nullopt;
		}

		std::scoped_lock lock{ g_lock };
		if (const auto byForm = g_byFormID.find(form->formID); byForm != g_byFormID.end())
		{
			return byForm->second;
		}

		const auto* editorID = form->GetFormEditorID();
		if (!editorID || editorID[0] == '\0')
		{
			return std::nullopt;
		}

		const auto byEditor = g_byEditorID.find(editorKey(editorID));
		return byEditor != g_byEditorID.end() ? std::optional{ byEditor->second } : std::nullopt;
	}

	std::optional<std::string> KnownFurnitureUseText()
	{
		std::scoped_lock lock{ g_lock };
		return g_knownFurnitureUseText;
	}
}
