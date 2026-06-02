// AI CONTEXT: Coordinates boot-time source-free mutation and cheap save-load runtime map refresh.
// Depends on RuntimePreload, RuntimeConstApplyCache, RuntimeApplyTrace, and runtime data apply modules.
// Runtime scope is Fallout 4 1.11.191 text mutation plus source-free lookup data readiness.
// Version-specific logic: Fallout 4 1.11.191 only; no alternate runtime branches.
// Source-free policy: orchestration uses plugin/FormID/editorID/index/sID identity; no Source matching.
#include "PCH.h"

#include "111191/RuntimeActivationTextTranslations.h"
#include "RuntimeApplySettings.h"
#include "111191/RuntimeApplyTrace.h"
#include "111191/RuntimeConstApplyCache.h"
#include "111191/RuntimeDescriptionTranslations.h"
#include "111191/RuntimeDialogueChoiceTranslations.h"
#include "111191/RuntimeDialogueDataApply.h"
#include "111191/RuntimeDialogueResponseTranslations.h"
#include "111191/RuntimeFormResolver.h"
#include "111191/RuntimeFullNameLoadTranslations.h"
#include "111191/RuntimeInventoryTemplateNames.h"
#include "111191/RuntimeInstanceNamingTranslations.h"
#include "RuntimeLoadWatchdog.h"
#include "111191/RuntimePipboyLogTranslations.h"
#include "111191/RuntimePerkActivateChoiceTranslations.h"
#include "111191/RuntimePreload.h"
#include "111191/RuntimeQuestLogTranslations.h"
#include "111191/RuntimeTextManager.h"
#include "111191/RuntimeXdiDialogueOptionTranslations.h"

#include <chrono>
#include <mutex>

namespace Runtime111191
{
namespace
{
	std::mutex g_applyLock;
	bool g_bootApplyComplete{ false };
	const TranslationPipelineResult* g_bootCatalogResult{ nullptr };
	bool g_bootCatalogLoadedFromCache{ false };

	[[nodiscard]] double elapsedMs(std::chrono::steady_clock::time_point started)
	{
		return static_cast<double>(
			std::chrono::duration_cast<std::chrono::microseconds>(
				std::chrono::steady_clock::now() - started)
				.count()) /
			1000.0;
	}

	void logPhase(std::string_view phase, std::chrono::steady_clock::time_point started)
	{
		REX::INFO("{} runtime phase complete: phase='{}' elapsedMs={:.2f}.", Plugin::NAME, phase, elapsedMs(started));
	}

	[[nodiscard]] const TranslationPipelineResult* catalogResultOrLogMissing(
		const RuntimeApplySettings::Values& settings)
	{
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "RuntimeTextManager BuildCatalogOnce", 0.0 };
			RuntimePreload::BuildCatalogOnce();
		}

		const auto* catalogResult = RuntimePreload::GetCatalogBuildResult();
		if (!catalogResult)
		{
			RuntimeApplyTrace::Summary(settings, "missing-catalog");
		}
		return catalogResult;
	}

	void rebuildRuntimeLookupMaps(const TranslationPreparedData& prepared, bool force)
	{
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "RuntimeTextManager rebuild quest journal translations", 0.0 };
			const auto started = std::chrono::steady_clock::now();
			RuntimeQuestLogTranslations::Rebuild(prepared.questJournal);
			logPhase("rebuild quest journal translations", started);
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "RuntimeTextManager rebuild pipboy-log GMST translations", 0.0 };
			const auto started = std::chrono::steady_clock::now();
			RuntimePipboyLogTranslations::Rebuild(prepared.pipboyLog);
			logPhase("rebuild pipboy-log GMST translations", started);
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "RuntimeTextManager rebuild description translations", 0.0 };
			const auto started = std::chrono::steady_clock::now();
			RuntimeDescriptionTranslations::Rebuild(prepared.description);
			logPhase("rebuild description translations", started);
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "RuntimeTextManager rebuild dialogue choice translations", 0.0 };
			const auto started = std::chrono::steady_clock::now();
			RuntimeDialogueChoiceTranslations::Rebuild(prepared.dialogue, force);
			logPhase("rebuild dialogue choice translations", started);
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "RuntimeTextManager rebuild dialogue response translations", 0.0 };
			const auto started = std::chrono::steady_clock::now();
			RuntimeDialogueResponseTranslations::Rebuild(prepared.dialogue, force);
			logPhase("rebuild dialogue response translations", started);
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "RuntimeTextManager rebuild XDI dialogue option translations", 0.0 };
			const auto started = std::chrono::steady_clock::now();
			RuntimeXdiDialogueOptionTranslations::Rebuild(prepared.dialogue, force);
			logPhase("rebuild XDI dialogue option translations", started);
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "RuntimeTextManager rebuild activation-text translations", 0.0 };
			const auto started = std::chrono::steady_clock::now();
			RuntimeActivationTextTranslations::Rebuild(prepared.activationText, force);
			logPhase("rebuild activation-text translations", started);
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "RuntimeTextManager rebuild perk activate-choice translations", 0.0 };
			const auto started = std::chrono::steady_clock::now();
			RuntimePerkActivateChoiceTranslations::Rebuild(prepared.perkActivateChoice, force);
			logPhase("rebuild perk activate-choice translations", started);
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "RuntimeTextManager rebuild load-time FULL translations", 0.0 };
			const auto started = std::chrono::steady_clock::now();
			RuntimeFullNameLoadTranslations::Rebuild(prepared.fullNameLoad, force);
			logPhase("rebuild load-time FULL translations", started);
		}
	}

	void applyHeavyDataSections(const TranslationPreparedData& prepared)
	{
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "RuntimeTextManager apply dialogue topic-name data", 0.0 };
			const auto started = std::chrono::steady_clock::now();
			(void)RuntimeDialogueDataApply::Apply(prepared.dialogue);
			logPhase("apply dialogue topic-name data", started);
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "RuntimeTextManager apply inventory template names", 0.0 };
			const auto started = std::chrono::steady_clock::now();
			(void)RuntimeInventoryTemplateNames::Apply(prepared.inventoryTemplateNames);
			logPhase("apply inventory template names", started);
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "RuntimeTextManager rebuild INNR translations", 0.0 };
			const auto started = std::chrono::steady_clock::now();
			(void)RuntimeInstanceNamingTranslations::Rebuild(prepared.instanceNaming);
			logPhase("rebuild INNR translations", started);
		}
	}

	void reapplySaveLoadMutableData(const TranslationPreparedData& prepared)
	{
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "RuntimeTextManager reapply dialogue topic-name data after save-load", 0.0 };
			const auto started = std::chrono::steady_clock::now();
			(void)RuntimeDialogueDataApply::Apply(prepared.dialogue);
			logPhase("reapply dialogue topic-name data after save-load", started);
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "RuntimeTextManager reapply inventory template names after save-load", 0.0 };
			const auto started = std::chrono::steady_clock::now();
			(void)RuntimeInventoryTemplateNames::Apply(prepared.inventoryTemplateNames, true);
			logPhase("reapply inventory template names after save-load", started);
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "RuntimeTextManager reapply INNR translations after save-load", 0.0 };
			const auto started = std::chrono::steady_clock::now();
			(void)RuntimeInstanceNamingTranslations::Reapply(prepared.instanceNaming);
			logPhase("reapply INNR translations after save-load", started);
		}
	}

	bool applyBootCatalogLocked(
		const RuntimeApplySettings::Values& settings,
		std::string_view summaryStage)
	{
		if (g_bootApplyComplete)
		{
			REX::INFO(
				"{} boot catalog apply already complete; skipping heavy apply cacheHit={}.",
				Plugin::NAME,
				g_bootCatalogLoadedFromCache);
			return true;
		}

		const auto* catalogResult = catalogResultOrLogMissing(settings);
		if (!catalogResult)
		{
			return false;
		}
		const auto& prepared = catalogResult->prepared;

		rebuildRuntimeLookupMaps(prepared, false);
		applyHeavyDataSections(prepared);
		(void)RuntimeConstApplyCache::BuildAndApply(prepared.constApply, settings, "boot");

		g_bootCatalogResult = catalogResult;
		g_bootCatalogLoadedFromCache = catalogResult->loadedFromCache;
		g_bootApplyComplete = true;
		RuntimeApplyTrace::Summary(settings, summaryStage);
		return true;
	}
}

namespace RuntimeTextManager
{
	void ApplyBootCatalogOnce()
	{
		std::scoped_lock lock{ g_applyLock };
		RuntimeLoadWatchdog::ScopedPhase totalPhase{ "RuntimeTextManager ApplyBootCatalogOnce total", 0.0 };
		const auto settings = RuntimeApplySettings::Load();
		RuntimeApplyTrace::Reset();
		(void)applyBootCatalogLocked(settings, "apply-catalog");
	}

	void RefreshAfterSaveLoad()
	{
		std::scoped_lock lock{ g_applyLock };
		RuntimeLoadWatchdog::ScopedPhase totalPhase{ "RuntimeTextManager RefreshAfterSaveLoad total", 0.0 };
		const auto settings = RuntimeApplySettings::Load();
		RuntimeApplyTrace::Reset();

		if (!g_bootApplyComplete)
		{
			REX::INFO("{} save-load refresh requires boot apply; applying catalog once.", Plugin::NAME);
			(void)applyBootCatalogLocked(settings, "save-load-boot-apply");
			return;
		}

		const auto* catalogResult = catalogResultOrLogMissing(settings);
		if (!catalogResult)
		{
			return;
		}

		const bool sameCatalog = catalogResult == g_bootCatalogResult;
		if (sameCatalog)
		{
			REX::INFO(
				"{} save-load refresh reapplying cached mutable data: bootApplied=true sameCatalog=true cacheHit={} bootCacheHit={}.",
				Plugin::NAME,
				catalogResult->loadedFromCache,
				g_bootCatalogLoadedFromCache);
			RuntimeFormResolver::ClearLiveFormCache();
			const auto constStats = RuntimeConstApplyCache::Reapply(
				catalogResult->prepared.constApply,
				settings,
				"same-catalog-save-load");
			reapplySaveLoadMutableData(catalogResult->prepared);
			REX::INFO(
				"{} save-load refresh complete: sameCatalog=true constMapsReused={} constMapsBuilt={} dialogueReapplied=true inventoryTemplatesReapplied=true innrReapplied=true cacheHit={}.",
				Plugin::NAME,
				constStats.mapsReused,
				constStats.mapsBuilt,
				catalogResult->loadedFromCache);
			RuntimeApplyTrace::Summary(settings, "save-load-cached-refresh");
			return;
		}

		REX::WARN("{} save-load refresh saw a different catalog pointer; running conservative refresh.", Plugin::NAME);
		RuntimeFormResolver::ClearLiveFormCache();
		rebuildRuntimeLookupMaps(catalogResult->prepared, true);
		(void)RuntimeConstApplyCache::BuildAndApply(catalogResult->prepared.constApply, settings, "different-catalog-save-load");
		reapplySaveLoadMutableData(catalogResult->prepared);
		g_bootCatalogResult = catalogResult;
		g_bootCatalogLoadedFromCache = catalogResult->loadedFromCache;

		REX::INFO(
			"{} save-load refresh complete: forceRebuiltMaps=true reappliedMutableData=true reappliedConstData=true bootApplied={} cacheHit={}.",
			Plugin::NAME,
			g_bootApplyComplete,
			catalogResult->loadedFromCache);
		RuntimeApplyTrace::Summary(settings, "save-load-refresh");
	}
}

} // namespace Runtime111191
