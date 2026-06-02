// AI CONTEXT: Builds source-free INFO:RNAM and DIAL:FULL maps for dialogue button text lookup.
// Depends on RuntimeFormResolver, RuntimeApplySettings, and TranslationCatalog records.
// Runtime scope is Fallout 4 1.10.163 dialogue choice prompt rendering.
// Version-specific logic: resolves 1.10.163 runtime form IDs from plugin/raw XML identity.
// Source-free policy: never reads XML Source or visible button text as identity.
#include "PCH.h"

#include "110163/RuntimeDialogueChoiceTranslations.h"

#include "RuntimeApplySettings.h"
#include "110163/RuntimeFormResolver.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace
{
	constexpr std::uint32_t kTraceLimit{ 96 };

	struct ChoiceSnapshot
	{
		std::unordered_map<std::uint32_t, std::string> promptsByInfo;
		std::unordered_map<std::uint32_t, std::string> topicsByForm;
		bool traceEnabled{ false };
	};

	std::mutex g_rebuildLock;
	const TranslationCatalogBuildResult* g_catalog{ nullptr };
	std::atomic<std::shared_ptr<const ChoiceSnapshot>> g_snapshot;
	std::atomic_uint32_t g_traceLines{ 0 };

	[[nodiscard]] bool hasText(std::string_view text)
	{
		return text.find_first_not_of(" \t\r\n") != std::string_view::npos;
	}

	[[nodiscard]] bool isPromptRecord(const TranslationCatalogRecord& record) noexcept
	{
		return record.recordSignature == "INFO RNAM" &&
			record.data.translationType == TranslationType::kRuntime2 &&
			hasText(record.data.replacerText);
	}

	[[nodiscard]] bool isTopicRecord(const TranslationCatalogRecord& record) noexcept
	{
		return record.recordSignature == "DIAL FULL" &&
			record.data.translationType == TranslationType::kRuntime1 &&
			hasText(record.data.replacerText);
	}

	[[nodiscard]] std::optional<std::uint32_t> runtimeFormID(const TranslationCatalogRecord& record)
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

	void traceResolve(
		const ChoiceSnapshot& snapshot,
		std::string_view stage,
		const RuntimeDialogueChoiceTranslations::ChoiceContext& context,
		std::uint32_t formID,
		std::size_t textLen)
	{
		if (!snapshot.traceEnabled || g_traceLines.fetch_add(1, std::memory_order_relaxed) >= kTraceLimit)
		{
			return;
		}
		REX::INFO(
			"{} dialogue-choice-map trace stage={} type={} info0={:08X} info1={:08X} topic={:08X} quest={:08X} form={:08X} textLen={}",
			Plugin::NAME,
			stage,
			context.responseType,
			context.infoCount > 0 ? context.infoFormIDs[0] : 0,
			context.infoCount > 1 ? context.infoFormIDs[1] : 0,
			context.topicFormID,
			context.questFormID,
			formID,
			textLen);
	}
}

namespace RuntimeDialogueChoiceTranslations
{
	void Rebuild(const TranslationCatalogBuildResult& catalog, bool force)
	{
		std::scoped_lock lock{ g_rebuildLock };
		if (!force && g_catalog == std::addressof(catalog))
		{
			return;
		}

		auto snapshot = std::make_shared<ChoiceSnapshot>();
		snapshot->traceEnabled = RuntimeApplySettings::Load().TraceEnabled();
		snapshot->promptsByInfo.reserve(catalog.records.size() / 8);
		snapshot->topicsByForm.reserve(catalog.records.size() / 32);

		for (const auto& record : catalog.records)
		{
			if (!isPromptRecord(record) && !isTopicRecord(record))
			{
				continue;
			}
			const auto formID = runtimeFormID(record);
			if (!formID)
			{
				continue;
			}
			if (isPromptRecord(record))
			{
				snapshot->promptsByInfo.insert_or_assign(*formID, record.data.replacerText);
			}
			else
			{
				snapshot->topicsByForm.insert_or_assign(*formID, record.data.replacerText);
			}
		}

		g_catalog = std::addressof(catalog);
		g_traceLines.store(0, std::memory_order_relaxed);
		g_snapshot.store(std::static_pointer_cast<const ChoiceSnapshot>(snapshot), std::memory_order_release);
		if (snapshot->traceEnabled)
		{
			REX::INFO(
				"{} dialogue-choice map built: prompts={} topics={}.",
				Plugin::NAME,
				snapshot->promptsByInfo.size(),
				snapshot->topicsByForm.size());
		}
	}

	LookupResult Resolve(const ChoiceContext& context)
	{
		const auto snapshot = g_snapshot.load(std::memory_order_acquire);
		if (!snapshot)
		{
			return {};
		}

		for (std::size_t i = 0; i < context.infoCount && i < context.infoFormIDs.size(); ++i)
		{
			const auto formID = context.infoFormIDs[i];
			if (const auto found = snapshot->promptsByInfo.find(formID); found != snapshot->promptsByInfo.end())
			{
				traceResolve(*snapshot, "hit-info", context, formID, found->second.size());
				return { found->second, "INFO:RNAM", formID, true };
			}
		}

		if (context.topicFormID != 0)
		{
			if (const auto found = snapshot->topicsByForm.find(context.topicFormID); found != snapshot->topicsByForm.end())
			{
				traceResolve(*snapshot, "hit-topic", context, context.topicFormID, found->second.size());
				return { found->second, "DIAL:FULL", context.topicFormID, true };
			}
		}

		traceResolve(*snapshot, "miss", context, 0, 0);
		return {};
	}
}
