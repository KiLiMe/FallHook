// AI CONTEXT: Implements compact hook timing summaries for the PreLoadGame/PostLoadGame wait.
// Depends on RuntimeLoadWatchdog for activation and REX logging for output.
// Runtime assumptions: version-neutral; hooked runtime code creates ScopedHook with stable names.
// Version-specific logic: none; the fixed hook-name list is diagnostic only.
// Source-free policy: records hook timing only; no original text or translated text is logged.
#include "PCH.h"

#include "RuntimeSaveLoadGapTrace.h"

#include "RuntimeLoadWatchdog.h"

#include <array>
#include <chrono>
#include <functional>
#include <mutex>
#include <thread>

namespace
{
	struct HookStats
	{
		std::string_view name;
		std::uint64_t calls{ 0 };
		std::uint64_t totalUs{ 0 };
		std::uint64_t maxUs{ 0 };
		std::uint64_t slowCalls{ 0 };
	};

	constexpr std::uint64_t kSlowWarnUs{ 10000 };
	constexpr std::uint64_t kSummaryEvery{ 2048 };

	std::mutex g_lock;
	std::array<HookStats, 8> g_stats{ {
		{ "BSScaleformTranslator::Translate" },
		{ "BSTranslator::AddTranslation" },
		{ "BSScaleformTranslator::AddTranslations" },
		{ "TESFullName::LoadFullNameChunk" },
		{ "DialogueResponse::DialogueResponse" },
		{ "TESDescription::GetDescription" },
		{ "PipboyLogData::PopulateStatsVisitor::Visit" },
		{ "TESQuestStageItem::GetLogEntry" },
	} };

	[[nodiscard]] std::int64_t clockUs()
	{
		return std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now().time_since_epoch())
			.count();
	}

	[[nodiscard]] std::size_t currentThreadHash()
	{
		return std::hash<std::thread::id>{}(std::this_thread::get_id());
	}

	[[nodiscard]] HookStats* findStats(std::string_view hookName) noexcept
	{
		for (auto& stats : g_stats)
		{
			if (stats.name == hookName)
			{
				return std::addressof(stats);
			}
		}
		return nullptr;
	}

	void clearStats() noexcept
	{
		for (auto& stats : g_stats)
		{
			stats.calls = 0;
			stats.totalUs = 0;
			stats.maxUs = 0;
			stats.slowCalls = 0;
		}
	}

	void recordHook(std::string_view hookName, std::uint64_t elapsedUs)
	{
		HookStats snapshot;
		bool emitSummary = false;
		bool emitSlow = false;
		{
			std::scoped_lock lock{ g_lock };
			auto* stats = findStats(hookName);
			if (!stats)
			{
				return;
			}
			++stats->calls;
			stats->totalUs += elapsedUs;
			stats->maxUs = std::max(stats->maxUs, elapsedUs);
			if (elapsedUs >= kSlowWarnUs)
			{
				++stats->slowCalls;
				emitSlow = true;
			}
			emitSummary = stats->calls % kSummaryEvery == 0;
			snapshot = *stats;
		}

		if (emitSlow)
		{
			REX::WARN(
				"{} save-load-gap hook slow name='{}' elapsed={:.2f} ms calls={} total={:.2f} ms max={:.2f} ms slow={} thread={}.",
				Plugin::NAME,
				hookName,
				static_cast<double>(elapsedUs) / 1000.0,
				snapshot.calls,
				static_cast<double>(snapshot.totalUs) / 1000.0,
				static_cast<double>(snapshot.maxUs) / 1000.0,
				snapshot.slowCalls,
				currentThreadHash());
		}
		else if (emitSummary)
		{
			REX::INFO(
				"{} save-load-gap hook running name='{}' calls={} total={:.2f} ms avg={} us max={:.2f} ms slow={}.",
				Plugin::NAME,
				hookName,
				snapshot.calls,
				static_cast<double>(snapshot.totalUs) / 1000.0,
				snapshot.calls ? snapshot.totalUs / snapshot.calls : 0,
				static_cast<double>(snapshot.maxUs) / 1000.0,
				snapshot.slowCalls);
		}
	}
}

namespace RuntimeSaveLoadGapTrace
{
	ScopedHook::ScopedHook(std::string_view hookName) noexcept :
		hookName_(hookName),
		active_(RuntimeLoadWatchdog::SaveLoadGapActive())
	{
		if (active_)
		{
			startUs_ = clockUs();
		}
	}

	ScopedHook::~ScopedHook()
	{
		if (!active_)
		{
			return;
		}
		const auto elapsedUs = clockUs() - startUs_;
		recordHook(hookName_, elapsedUs > 0 ? static_cast<std::uint64_t>(elapsedUs) : 0);
	}

	void Reset() noexcept
	{
		std::scoped_lock lock{ g_lock };
		clearStats();
	}

	void LogSummaryAndReset(std::string_view reason)
	{
		std::array<HookStats, g_stats.size()> snapshot;
		{
			std::scoped_lock lock{ g_lock };
			snapshot = g_stats;
			clearStats();
		}

		std::uint64_t totalCalls = 0;
		for (const auto& stats : snapshot)
		{
			totalCalls += stats.calls;
		}
		if (totalCalls == 0)
		{
			REX::INFO("{} save-load-gap hook summary reason={} no hook activity.", Plugin::NAME, reason);
			return;
		}

		for (const auto& stats : snapshot)
		{
			if (stats.calls == 0)
			{
				continue;
			}
			REX::INFO(
				"{} save-load-gap hook summary reason={} name='{}' calls={} total={:.2f} ms avg={} us max={:.2f} ms slow={}.",
				Plugin::NAME,
				reason,
				stats.name,
				stats.calls,
				static_cast<double>(stats.totalUs) / 1000.0,
				stats.totalUs / stats.calls,
				static_cast<double>(stats.maxUs) / 1000.0,
				stats.slowCalls);
		}
	}
}
