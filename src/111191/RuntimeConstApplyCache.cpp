// AI CONTEXT: Implements cached direct const text map build and source-free reapply.
// Depends on ConstApplyMap, RuntimeFormResolver, RuntimeTextApply, RuntimeApplyTrace, and watchdog logging.
// Runtime scope is Fallout 4 1.11.191 direct mutable game-data fields.
// Version-specific logic: Fallout 4 1.11.191 only; no alternate runtime branches.
// Source-free policy: applies destination text from cached source-free identity maps; never matches Source text.
#include "PCH.h"

#include "ConstApplyMap.h"
#include "RuntimeLoadWatchdog.h"
#include "111191/RuntimeApplyTrace.h"
#include "111191/RuntimeConstApplyCache.h"
#include "111191/RuntimeFormResolver.h"
#include "111191/RuntimeTextApply.h"

#include <chrono>
#include <mutex>
#include <optional>

namespace Runtime111191
{
namespace
{
	std::mutex g_lock;
	const TranslationCatalogBuildResult* g_cachedCatalog{ nullptr };
	std::optional<ConstApplyMaps> g_cachedMaps;

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

	[[nodiscard]] RuntimeConstApplyCache::ApplyStats makeStats(const ConstApplyMaps& maps)
	{
		RuntimeConstApplyCache::ApplyStats stats;
		stats.formMapEntries = maps.stats.formEntries;
		stats.editorIDFormEntries = maps.stats.editorIDFormEntries;
		stats.gameSettingEntries = maps.stats.gameSettingEntries;
		stats.skippedUnsupportedType = maps.stats.skippedUnsupportedType;
		stats.skippedEmptyText = maps.stats.skippedEmptyText;
		stats.skippedMissingTarget = maps.stats.skippedMissingTarget;
		return stats;
	}

	ConstApplyMaps& mapsForCatalogLocked(
		const TranslationCatalogBuildResult& catalog,
		RuntimeConstApplyCache::ApplyStats& stats)
	{
		if (g_cachedCatalog != std::addressof(catalog) || !g_cachedMaps)
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "RuntimeConstApplyCache build const apply maps", 0.0 };
			const auto started = std::chrono::steady_clock::now();
			g_cachedMaps = ConstApplyMap::Build(catalog);
			g_cachedCatalog = std::addressof(catalog);
			stats.mapsBuilt = true;
			logPhase("build const apply maps", started);
		}
		else
		{
			stats.mapsReused = true;
		}
		return *g_cachedMaps;
	}

	bool skipNotAllowed(
		const ConstApplyEntry& entry,
		RuntimeConstApplyCache::ApplyStats& stats,
		const RuntimeApplySettings::Values& settings,
		std::string_view target,
		std::uint32_t rawFormID)
	{
		if (RuntimeTextApply::IsAllowed(entry.data.translationType))
		{
			return false;
		}

		RuntimeApplyTrace::Entry(settings, "skip-not-allowed", target, rawFormID, entry);
		++stats.skippedNotAllowed;
		return true;
	}

	void applyResolvedForm(
		RE::TESForm* form,
		const ConstApplyEntry& entry,
		RuntimeConstApplyCache::ApplyStats& stats,
		const RuntimeApplySettings::Values& settings,
		std::string_view target,
		std::uint32_t rawFormID)
	{
		if (!form)
		{
			RuntimeApplyTrace::Entry(settings, "skip-missing-form", target, rawFormID, entry);
			++stats.skippedMissingForm;
			return;
		}

		RuntimeApplyTrace::Entry(settings, "apply-begin", target, rawFormID, entry, form);
		if (RuntimeTextApply::ApplyForm(form, entry.data))
		{
			RuntimeApplyTrace::Entry(settings, "apply-ok", target, rawFormID, entry, form);
			++stats.applied;
		}
		else
		{
			RuntimeApplyTrace::Entry(settings, "apply-invalid-target", target, rawFormID, entry, form);
			++stats.skippedInvalidTarget;
		}
	}

	void applyGameSetting(
		const ConstApplyEntry& entry,
		RuntimeConstApplyCache::ApplyStats& stats,
		const RuntimeApplySettings::Values& settings)
	{
		if (skipNotAllowed(entry, stats, settings, "gmst", 0))
		{
			return;
		}

		RuntimeApplyTrace::Entry(settings, "apply-begin", "gmst", 0, entry);
		if (RuntimeTextApply::ApplyGameSetting(entry.data))
		{
			RuntimeApplyTrace::Entry(settings, "apply-ok", "gmst", 0, entry);
			++stats.applied;
		}
		else
		{
			RuntimeApplyTrace::Entry(settings, "apply-invalid-target", "gmst", 0, entry);
			++stats.skippedInvalidTarget;
		}
	}

	void applyConstMaps(
		const ConstApplyMaps& maps,
		RuntimeConstApplyCache::ApplyStats& stats,
		const RuntimeApplySettings::Values& settings)
	{
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "RuntimeConstApplyCache apply form entries", 0.0 };
			const auto started = std::chrono::steady_clock::now();
			for (const auto& [rawFormID, entry] : maps.formEntries)
			{
				if (skipNotAllowed(entry, stats, settings, "form", rawFormID))
				{
					continue;
				}

				RuntimeApplyTrace::Entry(settings, "resolve-begin", "form", rawFormID, entry);
				auto* form = RuntimeFormResolver::ResolveRawForm(rawFormID, entry.pluginName);
				RuntimeApplyTrace::Entry(settings, "resolve-end", "form", rawFormID, entry, form);
				applyResolvedForm(form, entry, stats, settings, "form", rawFormID);
			}
			logPhase("apply const form entries", started);
		}

		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "RuntimeConstApplyCache apply editorID entries", 0.0 };
			const auto started = std::chrono::steady_clock::now();
			for (const auto& entry : maps.editorIDFormEntries)
			{
				if (skipNotAllowed(entry, stats, settings, "editor", 0))
				{
					continue;
				}

				RuntimeApplyTrace::Entry(settings, "resolve-begin", "editor", 0, entry);
				auto* form = RuntimeFormResolver::ResolveEditorForm(entry.data);
				RuntimeApplyTrace::Entry(settings, "resolve-end", "editor", 0, entry, form);
				applyResolvedForm(form, entry, stats, settings, "editor", 0);
			}
			logPhase("apply const editorID entries", started);
		}

		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "RuntimeConstApplyCache apply game settings", 0.0 };
			const auto started = std::chrono::steady_clock::now();
			for (const auto& [_, entry] : maps.gameSettingEntries)
			{
				applyGameSetting(entry, stats, settings);
			}
			logPhase("apply const game settings", started);
		}
	}

	RuntimeConstApplyCache::ApplyStats applyLocked(
		const TranslationCatalogBuildResult& catalog,
		const RuntimeApplySettings::Values& settings,
		std::string_view reason)
	{
		RuntimeConstApplyCache::ApplyStats stats;
		const auto& maps = mapsForCatalogLocked(catalog, stats);
		const auto built = stats.mapsBuilt;
		const auto reused = stats.mapsReused;
		stats = makeStats(maps);
		stats.mapsBuilt = built;
		stats.mapsReused = reused;

		REX::INFO(
			"{} const cached apply begin: reason={} formEntries={} editorIDEntries={} gameSettings={} mapsBuilt={} mapsReused={} trace={}.",
			Plugin::NAME,
			reason,
			stats.formMapEntries,
			stats.editorIDFormEntries,
			stats.gameSettingEntries,
			stats.mapsBuilt,
			stats.mapsReused,
			settings.TraceEnabled());

		const auto started = std::chrono::steady_clock::now();
		applyConstMaps(maps, stats, settings);
		REX::INFO(
			"{} const cached apply complete: reason={} applied={} notAllowed={} missingForm={} invalidTarget={} unsupportedCatalog={} emptyText={} missingTarget={} elapsedMs={:.2f}.",
			Plugin::NAME,
			reason,
			stats.applied,
			stats.skippedNotAllowed,
			stats.skippedMissingForm,
			stats.skippedInvalidTarget,
			stats.skippedUnsupportedType,
			stats.skippedEmptyText,
			stats.skippedMissingTarget,
			elapsedMs(started));
		return stats;
	}
}

namespace RuntimeConstApplyCache
{
	ApplyStats BuildAndApply(
		const TranslationCatalogBuildResult& catalog,
		const RuntimeApplySettings::Values& settings,
		std::string_view reason)
	{
		std::scoped_lock lock{ g_lock };
		return applyLocked(catalog, settings, reason);
	}

	ApplyStats Reapply(
		const TranslationCatalogBuildResult& catalog,
		const RuntimeApplySettings::Values& settings,
		std::string_view reason)
	{
		std::scoped_lock lock{ g_lock };
		if (g_cachedCatalog != std::addressof(catalog) || !g_cachedMaps)
		{
			REX::WARN("{} const reapply cache miss; rebuilding maps for reason={}.", Plugin::NAME, reason);
			return applyLocked(catalog, settings, reason);
		}
		return applyLocked(catalog, settings, reason);
	}
}

} // namespace Runtime111191
