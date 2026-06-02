// AI CONTEXT: Implements per-hook timing diagnostics for runtime hook stall attribution.
// Depends on PCH/REX logging and is enabled by the independent [Watchdog] switch.
// Runtime assumptions: version-neutral diagnostics for whichever module is active.
// Version-specific logic: none; target-specific hook names are provided by hook modules.
// Source-free policy: logs hook identity and timing only; no original text lookup or matching.
#include "PCH.h"

#include "RuntimeHookWatch.h"

#include "RuntimeActivityWatch.h"

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
	struct RegisteredHook
	{
		RuntimeHookWatch::Stats* stats{ nullptr };
		std::string_view name;
	};

	std::atomic_bool g_enabled{ false };
	std::atomic_bool g_started{ false };
	std::atomic_bool g_stop{ false };
	std::atomic_uint64_t g_activeSequence{ 0 };
	std::mutex g_hookLock;
	std::mutex g_threadLock;
	std::vector<RegisteredHook> g_hooks;
	std::unique_ptr<std::thread> g_thread;

	constexpr std::uint32_t kSlowWarnMs = 50;
	constexpr std::uint32_t kActiveWarnMs = 500;
	constexpr std::uint64_t kSummaryEveryCalls = 2048;
	constexpr std::uint32_t kSlowLogLimit = 16;
	constexpr std::int64_t kSlowLogIntervalMs = 1000;
	constexpr auto kPollInterval = std::chrono::milliseconds(250);

	[[nodiscard]] std::int64_t clockMs()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch())
			.count();
	}

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

	[[nodiscard]] std::uint64_t avgUs(std::uint64_t totalUs, std::uint64_t calls) noexcept
	{
		return calls == 0 ? 0 : totalUs / calls;
	}

	[[nodiscard]] RuntimeActivityWatch::ScopeKind activityKind(std::string_view name) noexcept
	{
		return name.find("FallHook work") == std::string_view::npos ?
			RuntimeActivityWatch::ScopeKind::kHook :
			RuntimeActivityWatch::ScopeKind::kWork;
	}

	void registerHook(RuntimeHookWatch::Stats& stats, std::string_view name)
	{
		bool expected = false;
		if (!stats.registered.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
		{
			return;
		}

		std::scoped_lock lock{ g_hookLock };
		g_hooks.push_back(RegisteredHook{ std::addressof(stats), name });
	}

	void updateMaxMs(RuntimeHookWatch::Stats& stats, std::uint32_t elapsedMs)
	{
		auto current = stats.maxMs.load(std::memory_order_relaxed);
		while (elapsedMs > current &&
			   !stats.maxMs.compare_exchange_weak(current, elapsedMs, std::memory_order_relaxed))
		{}
	}

	void reportSlow(RuntimeHookWatch::Stats& stats, std::string_view hookName, std::uint32_t elapsedMs, std::uint64_t calls)
	{
		const auto slowCalls = stats.slowCalls.fetch_add(1, std::memory_order_relaxed) + 1;
		auto previousLogs = stats.slowLogs.load(std::memory_order_relaxed);
		if (previousLogs > kSlowLogLimit)
		{
			return;
		}

		const auto nowMs = clockMs();
		auto lastLogMs = stats.lastSlowLogMs.load(std::memory_order_relaxed);
		if (previousLogs != 0 && nowMs - lastLogMs < kSlowLogIntervalMs)
		{
			return;
		}
		if (!stats.lastSlowLogMs.compare_exchange_strong(lastLogMs, nowMs, std::memory_order_relaxed))
		{
			return;
		}

		previousLogs = stats.slowLogs.fetch_add(1, std::memory_order_relaxed);
		if (previousLogs < kSlowLogLimit)
		{
			const auto totalUs = stats.totalUs.load(std::memory_order_relaxed);
			REX::WARN(
				"{} hook-watch stage=slow name='{}' elapsed={} ms calls={} slow={} total={} ms avg={} us max={} ms thread={}.",
				Plugin::NAME,
				hookName,
				elapsedMs,
				calls,
				slowCalls,
				totalUs / 1000,
				avgUs(totalUs, calls),
				stats.maxMs.load(std::memory_order_relaxed),
				currentThreadHash());
		}
		else if (previousLogs == kSlowLogLimit)
		{
			const auto totalUs = stats.totalUs.load(std::memory_order_relaxed);
			REX::WARN(
				"{} hook-watch stage=slow-limit name='{}' cap={} calls={} slow={} total={} ms avg={} us max={} ms.",
				Plugin::NAME,
				hookName,
				kSlowLogLimit,
				calls,
				slowCalls,
				totalUs / 1000,
				avgUs(totalUs, calls),
				stats.maxMs.load(std::memory_order_relaxed));
		}
	}

	void reportSummary(RuntimeHookWatch::Stats& stats, std::string_view hookName, std::uint64_t calls)
	{
		const auto totalUs = stats.totalUs.load(std::memory_order_relaxed);
		REX::INFO(
			"{} hook-watch stage=summary name='{}' calls={} slow={} total={} ms avg={} us max={} ms thread={}.",
			Plugin::NAME,
			hookName,
			calls,
			stats.slowCalls.load(std::memory_order_relaxed),
			totalUs / 1000,
			avgUs(totalUs, calls),
			stats.maxMs.load(std::memory_order_relaxed),
			currentThreadHash());
	}

	void reportActiveHooks(std::int64_t nowMs)
	{
		std::vector<RegisteredHook> hooks;
		{
			std::scoped_lock lock{ g_hookLock };
			hooks = g_hooks;
		}

		for (const auto& hook : hooks)
		{
			if (!hook.stats)
			{
				continue;
			}

			const auto startMs = hook.stats->activeSinceMs.load(std::memory_order_acquire);
			if (startMs == 0 || nowMs - startMs < kActiveWarnMs)
			{
				continue;
			}

			bool expected = false;
			if (!hook.stats->activeReported.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
			{
				continue;
			}

			REX::WARN(
				"{} hook-watch stage=active-stuck name='{}' elapsed={} ms active={} calls={} slow={} total={} ms avg={} us max={} ms thread={}.",
				Plugin::NAME,
				hook.name,
				nowMs - startMs,
				hook.stats->activeSequence.load(std::memory_order_relaxed),
				hook.stats->calls.load(std::memory_order_relaxed),
				hook.stats->slowCalls.load(std::memory_order_relaxed),
				hook.stats->totalUs.load(std::memory_order_relaxed) / 1000,
				avgUs(
					hook.stats->totalUs.load(std::memory_order_relaxed),
					hook.stats->calls.load(std::memory_order_relaxed)),
				hook.stats->maxMs.load(std::memory_order_relaxed),
				hook.stats->activeThread.load(std::memory_order_relaxed));
		}
	}

	void startThread()
	{
		bool expected = false;
		if (!g_started.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
		{
			return;
		}

		g_stop.store(false, std::memory_order_release);
		auto thread = std::make_unique<std::thread>([]() {
			while (!g_stop.load(std::memory_order_acquire))
			{
				std::this_thread::sleep_for(kPollInterval);
				if (!g_enabled.load(std::memory_order_acquire) || g_stop.load(std::memory_order_acquire))
				{
					continue;
				}
				reportActiveHooks(clockMs());
			}
		});

		std::scoped_lock lock{ g_threadLock };
		g_thread = std::move(thread);
	}
}

namespace RuntimeHookWatch
{
	void Configure(bool enabled) noexcept
	{
		g_enabled.store(enabled, std::memory_order_release);
		if (enabled)
		{
			startThread();
			REX::INFO(
				"{} hook-watch active: slowWarn={} ms activeWarn={} ms summaryEvery={} calls slowLogCap={}.",
				Plugin::NAME,
				kSlowWarnMs,
				kActiveWarnMs,
				kSummaryEveryCalls,
				kSlowLogLimit);
		}
	}

	void Shutdown()
	{
		g_enabled.store(false, std::memory_order_release);
		g_stop.store(true, std::memory_order_release);

		std::unique_ptr<std::thread> thread;
		{
			std::scoped_lock lock{ g_threadLock };
			thread = std::move(g_thread);
		}

		if (!thread || !thread->joinable())
		{
			return;
		}
		if (thread->get_id() == std::this_thread::get_id())
		{
			thread->detach();
			return;
		}
		thread->join();
	}

	bool Enabled() noexcept
	{
		return g_enabled.load(std::memory_order_acquire);
	}

	ScopedCall::ScopedCall(Stats& stats, std::string_view hookName) noexcept :
		stats_(&stats),
		hookName_(hookName),
		activity_(RuntimeActivityWatch::Begin(activityKind(hookName), hookName)),
		startMs_(Enabled() ? clockMs() : 0),
		startUs_(startMs_ != 0 ? clockUs() : 0),
		active_(startMs_ != 0)
	{
		if (!active_)
		{
			return;
		}

		registerHook(stats, hookName);
		activeSequence_ = g_activeSequence.fetch_add(1, std::memory_order_acq_rel) + 1;
		stats.activeThread.store(static_cast<std::uint64_t>(currentThreadHash()), std::memory_order_relaxed);
		stats.activeSequence.store(activeSequence_, std::memory_order_release);
		stats.activeReported.store(false, std::memory_order_release);
		stats.activeSinceMs.store(startMs_, std::memory_order_release);
	}

	ScopedCall::~ScopedCall()
	{
		RuntimeActivityWatch::End(activity_);
		if (!active_ || !stats_)
		{
			return;
		}

		const auto elapsedUsRaw = clockUs() - startUs_;
		const auto elapsedUs = elapsedUsRaw <= 0 ? 0u : static_cast<std::uint64_t>(elapsedUsRaw);
		const auto elapsedMs = static_cast<std::uint32_t>((elapsedUs + 999u) / 1000u);
		const auto calls = stats_->calls.fetch_add(1, std::memory_order_relaxed) + 1;
		stats_->totalUs.fetch_add(elapsedUs, std::memory_order_relaxed);
		updateMaxMs(*stats_, elapsedMs);
		if (stats_->activeSequence.load(std::memory_order_acquire) == activeSequence_)
		{
			stats_->activeSinceMs.store(0, std::memory_order_release);
			stats_->activeReported.store(false, std::memory_order_release);
		}
		if (elapsedUs >= static_cast<std::uint64_t>(kSlowWarnMs) * 1000u)
		{
			reportSlow(*stats_, hookName_, elapsedMs, calls);
		}
		if (calls % kSummaryEveryCalls == 0)
		{
			reportSummary(*stats_, hookName_, calls);
		}
	}
}
