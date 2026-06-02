// AI CONTEXT: Mutates loaded dialogue topic-name data from source-free XML DIAL:FULL records.
// Depends on RuntimeDialogueForms, RuntimeFormResolver, and CommonLibF4 TESTopic.
// Runtime scope is Fallout 4 1.10.163 DIAL:FULL catalog apply only.
// Version-specific logic: resolves 1.10.163 runtime form IDs from plugin/raw XML identity.
// Source-free policy: applies by loaded DIAL identity; INFO:RNAM choices are handled by button/context hooks.
#include "PCH.h"

#include "110163/RuntimeDialogueDataApply.h"

#include "RuntimeApplySettings.h"
#include "110163/RuntimeDialogueForms.h"
#include "110163/RuntimeFormResolver.h"
#include "110163/RuntimeTextStringAssign.h"
#include "SourceFreeTranslationKey.h"
#include "RE/T/TESTopic.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace
{
	constexpr std::uint32_t kTraceLimit{ 192 };

	struct TopicTarget
	{
		const TranslationCatalogRecord* record{ nullptr };
		bool visited{ false };
	};

	struct DialogueMaps
	{
		std::unordered_map<std::uint32_t, TopicTarget> topicsByForm;
		std::unordered_map<std::string, TopicTarget> topicsByEditor;
		RuntimeDialogueDataApply::ApplyStats stats;
	};

	std::atomic_uint32_t g_traceLines{ 0 };
	std::mutex g_mapsLock;
	const TranslationCatalogBuildResult* g_cachedCatalog{ nullptr };
	std::optional<DialogueMaps> g_cachedMaps;

	[[nodiscard]] double elapsedMs(std::chrono::steady_clock::time_point started)
	{
		const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - started);
		return static_cast<double>(elapsed.count()) / 1000.0;
	}

	[[nodiscard]] bool hasText(std::string_view text)
	{
		return text.find_first_not_of(" \t\r\n") != std::string_view::npos;
	}

	void trace(
		const RuntimeApplySettings::Values& settings,
		std::string_view stage,
		const TranslationCatalogRecord& record,
		const RE::TESForm* form)
	{
		if (!settings.TraceEnabled() || g_traceLines.fetch_add(1, std::memory_order_relaxed) >= kTraceLimit)
		{
			return;
		}

		REX::INFO(
			"{} dialogue-topic-apply trace stage={} rec={} form={:08X} editor={} sid={:08X} index={} textLen={}",
			Plugin::NAME,
			stage,
			record.recordSignature,
			form ? form->formID : 0,
			form && form->GetFormEditorID() ? form->GetFormEditorID() : "",
			record.data.stringID.value_or(0),
			record.data.index.value_or(0xFFFFFFFFu),
			record.data.replacerText.size());
	}

	[[nodiscard]] bool isTopicRecord(const TranslationCatalogRecord& record)
	{
		return record.recordSignature == "DIAL FULL" &&
			record.data.translationType == TranslationType::kRuntime1;
	}

	[[nodiscard]] std::string editorKey(std::string_view editorID)
	{
		return SourceFreeTranslationKeys::NormalizeEditorID(editorID);
	}

	void addTopicTarget(DialogueMaps& maps, const TranslationCatalogRecord& record)
	{
		if (record.data.formID)
		{
			if (const auto formID = RuntimeFormResolver::ResolveRawFormID(*record.data.formID, record.pluginName))
			{
				maps.topicsByForm.insert_or_assign(*formID, TopicTarget{ std::addressof(record), false });
				return;
			}
		}
		if (record.data.editorID && !record.data.editorID->empty())
		{
			if (auto* form = RuntimeFormResolver::ResolveEditorForm(record.data))
			{
				if (auto* topic = form->As<RE::TESTopic>())
				{
					maps.topicsByForm.insert_or_assign(topic->formID, TopicTarget{ std::addressof(record), false });
					return;
				}
			}
			maps.topicsByEditor.insert_or_assign(editorKey(*record.data.editorID), TopicTarget{ std::addressof(record), false });
			return;
		}

		++maps.stats.skippedMissingForm;
	}

	[[nodiscard]] DialogueMaps buildMaps(const TranslationCatalogBuildResult& catalog)
	{
		DialogueMaps maps;
		maps.topicsByForm.reserve(catalog.records.size() / 32);
		for (const auto& record : catalog.records)
		{
			if (!isTopicRecord(record))
			{
				++maps.stats.skippedWrongType;
				continue;
			}
			if (!hasText(record.data.replacerText))
			{
				++maps.stats.skippedEmptyText;
				continue;
			}
			addTopicTarget(maps, record);
		}
		return maps;
	}

	void resetVisited(DialogueMaps& maps)
	{
		for (auto& [_, target] : maps.topicsByForm)
		{
			target.visited = false;
		}
		for (auto& [_, target] : maps.topicsByEditor)
		{
			target.visited = false;
		}
	}

	[[nodiscard]] DialogueMaps& mapsForCatalog(const TranslationCatalogBuildResult& catalog, bool reset)
	{
		if (g_cachedCatalog != std::addressof(catalog) || !g_cachedMaps)
		{
			g_cachedMaps = buildMaps(catalog);
			g_cachedCatalog = std::addressof(catalog);
		}
		if (reset)
		{
			resetVisited(*g_cachedMaps);
		}
		return *g_cachedMaps;
	}

	[[nodiscard]] TopicTarget* findEditorTopicTarget(DialogueMaps& maps, RE::TESTopic& topic)
	{
		if (maps.topicsByForm.contains(topic.formID))
		{
			return nullptr;
		}
		const auto* editorID = topic.GetFormEditorID();
		if (!editorID || editorID[0] == '\0')
		{
			return nullptr;
		}
		const auto found = maps.topicsByEditor.find(editorKey(editorID));
		return found != maps.topicsByEditor.end() ? std::addressof(found->second) : nullptr;
	}

	void applyTopicTarget(
		RE::TESTopic& topic,
		TopicTarget& target,
		const RuntimeApplySettings::Values& settings,
		RuntimeDialogueDataApply::ApplyStats& stats)
	{
		if (!target.record)
		{
			return;
		}

		target.visited = true;
		const auto& record = *target.record;
		RuntimeTextStringAssign::AssignPlainLocalized(topic.fullName, record.data.replacerText);
		++stats.topicsApplied;
		trace(settings, "topic-ok", record, std::addressof(topic));
	}

	void applyDirectTargets(
		DialogueMaps& maps,
		const RuntimeApplySettings::Values& settings,
		RuntimeDialogueDataApply::ApplyStats& stats)
	{
		for (auto& [formID, target] : maps.topicsByForm)
		{
			if (auto* topic = RE::TESForm::GetFormByID<RE::TESTopic>(formID))
			{
				applyTopicTarget(*topic, target, settings, stats);
			}
		}
	}

	[[nodiscard]] RuntimeDialogueForms::LoadedForms applyEditorFallbackTargets(
		DialogueMaps& maps,
		const RuntimeApplySettings::Values& settings,
		RuntimeDialogueDataApply::ApplyStats& stats)
	{
		RuntimeDialogueForms::LoadedForms forms;
		if (maps.topicsByEditor.empty())
		{
			return forms;
		}

		forms = RuntimeDialogueForms::Collect();
		for (auto* topic : forms.topics)
		{
			if (topic)
			{
				if (auto* target = findEditorTopicTarget(maps, *topic))
				{
					applyTopicTarget(*topic, *target, settings, stats);
				}
			}
		}
		return forms;
	}

	void countMissingTargets(DialogueMaps& maps, RuntimeDialogueDataApply::ApplyStats& stats)
	{
		for (const auto& [_, target] : maps.topicsByForm)
		{
			if (!target.visited)
			{
				++stats.skippedMissingForm;
			}
		}
		for (const auto& [_, target] : maps.topicsByEditor)
		{
			if (!target.visited)
			{
				++stats.skippedMissingForm;
			}
		}
	}
}

namespace RuntimeDialogueDataApply
{
	ApplyStats Apply(const TranslationCatalogBuildResult& catalog)
	{
		const auto started = std::chrono::steady_clock::now();
		const auto settings = RuntimeApplySettings::Load();
		g_traceLines.store(0, std::memory_order_relaxed);

		std::scoped_lock mapsLock{ g_mapsLock };
		auto& maps = mapsForCatalog(catalog, true);
		auto stats = maps.stats;

		applyDirectTargets(maps, settings, stats);
		const auto forms = applyEditorFallbackTargets(maps, settings, stats);
		countMissingTargets(maps, stats);

		REX::INFO(
			"{} dialogue topic apply complete: topics={} missingForm={} wrongType={} emptyText={} topicMaps={} scannedForms={} topicForms={} duplicateInfos={} elapsedMs={:.2f}.",
			Plugin::NAME,
			stats.topicsApplied,
			stats.skippedMissingForm,
			stats.skippedWrongType,
			stats.skippedEmptyText,
			maps.topicsByForm.size() + maps.topicsByEditor.size(),
			forms.scannedForms,
			forms.topics.size(),
			forms.duplicateInfos,
			elapsedMs(started));
		return stats;
	}
}
