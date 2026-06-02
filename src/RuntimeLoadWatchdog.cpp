// AI CONTEXT: Implements the loading watchdog thread and scoped load phase tracing.
// Depends on PCH/REX logging and is enabled by main from independent [Watchdog] settings.
// Runtime assumptions: version-neutral timing diagnostics for the selected runtime module.
// Version-specific logic: none; phase names come from active modules.
// Source-free policy: logs operational phase names and timings only, never original text keys.
#include "PCH.h"

#include "RuntimeLoadWatchdog.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace
{
	std::atomic_bool g_enabled{ false };
	std::atomic_bool g_started{ false };
	std::atomic_bool g_stop{ false };
	std::atomic_bool g_saveLoadGapActive{ false };
	std::atomic_uint64_t g_phaseSequence{ 0 };
	std::mutex g_phaseLock;
	std::mutex g_threadLock;
	std::unique_ptr<std::thread> g_thread;
	std::string g_phase;
	std::uint64_t g_phaseID{ 0 };
	std::int64_t g_phaseStartMs{ 0 };
	std::int64_t g_phaseLastWarnMs{ 0 };

	constexpr std::int64_t kStuckWarnMs = 2000;
	constexpr std::int64_t kRepeatWarnMs = 5000;
	constexpr auto kPollInterval = std::chrono::milliseconds(250);

	[[nodiscard]] std::int64_t clockMs()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch())
			.count();
	}

	[[nodiscard]] std::size_t currentThreadHash()
	{
		return std::hash<std::thread::id>{}(std::this_thread::get_id());
	}

	void reportActivePhase(std::int64_t nowMs)
	{
		std::string phase;
		std::uint64_t id = 0;
		std::int64_t startMs = 0;
		std::int64_t previousWarnMs = 0;
		{
			std::scoped_lock lock{ g_phaseLock };
			if (g_phase.empty() || g_phaseStartMs == 0)
			{
				return;
			}

			const auto elapsedMs = nowMs - g_phaseStartMs;
			if (elapsedMs < kStuckWarnMs)
			{
				return;
			}
			if (g_phaseLastWarnMs != 0 && nowMs - g_phaseLastWarnMs < kRepeatWarnMs)
			{
				return;
			}

			phase = g_phase;
			id = g_phaseID;
			startMs = g_phaseStartMs;
			previousWarnMs = g_phaseLastWarnMs;
			g_phaseLastWarnMs = nowMs;
		}

		REX::WARN(
			"{} load-watch stage=stuck id={} phase='{}' elapsed={} ms{}.",
			Plugin::NAME,
			id,
			phase,
			nowMs - startMs,
			previousWarnMs == 0 ? "" : " repeated");
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
				if (g_stop.load(std::memory_order_acquire) || !g_enabled.load(std::memory_order_acquire))
				{
					continue;
				}
				reportActivePhase(clockMs());
			}
		});

		{
			std::scoped_lock lock{ g_threadLock };
			g_thread = std::move(thread);
		}

		REX::INFO(
			"{} load-watch active: stuckWarn={} ms repeatWarn={} ms poll={} ms.",
			Plugin::NAME,
			kStuckWarnMs,
			kRepeatWarnMs,
			kPollInterval.count());
	}

	void clearCurrentPhase(std::uint64_t id)
	{
		std::scoped_lock lock{ g_phaseLock };
		if (g_phaseID != id)
		{
			return;
		}

		g_phase.clear();
		g_phaseID = 0;
		g_phaseStartMs = 0;
		g_phaseLastWarnMs = 0;
	}
}

namespace RuntimeLoadWatchdog
{
	void Configure(bool enabled)
	{
		g_enabled.store(enabled, std::memory_order_release);
		if (enabled)
		{
			startThread();
		}
	}

	void Shutdown()
	{
		g_saveLoadGapActive.store(false, std::memory_order_release);
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

	void SetSaveLoadGapActive(bool active) noexcept
	{
		g_saveLoadGapActive.store(active, std::memory_order_release);
	}

	bool SaveLoadGapActive() noexcept
	{
		return g_saveLoadGapActive.load(std::memory_order_acquire);
	}

	PhaseToken BeginPhase(std::string_view phase)
	{
		if (!Enabled())
		{
			return {};
		}

		const auto nowMs = clockMs();
		const auto id = g_phaseSequence.fetch_add(1, std::memory_order_acq_rel) + 1;
		{
			std::scoped_lock lock{ g_phaseLock };
			g_phase.assign(phase);
			g_phaseID = id;
			g_phaseStartMs = nowMs;
			g_phaseLastWarnMs = 0;
		}

		REX::INFO(
			"{} load-watch stage=begin id={} phase='{}' thread={}.",
			Plugin::NAME,
			id,
			phase,
			currentThreadHash());
		return PhaseToken{ id, nowMs };
	}

	void EndPhase(std::string_view phase, PhaseToken token, double logAfterMs)
	{
		if (!Enabled() || token.id == 0)
		{
			return;
		}

		const auto nowMs = clockMs();
		const auto elapsedMs = static_cast<double>(nowMs - token.startMs);
		clearCurrentPhase(token.id);
		if (logAfterMs > 0.0 && elapsedMs < logAfterMs)
		{
			return;
		}

		REX::INFO(
			"{} load-watch stage=end id={} phase='{}' elapsed={:.2f} ms thread={}.",
			Plugin::NAME,
			token.id,
			phase,
			elapsedMs,
			currentThreadHash());
	}

	void SetWaitPhase(std::string_view phase)
	{
		(void)BeginPhase(phase);
	}

	void ClearWaitPhase(std::string_view phase)
	{
		if (!Enabled())
		{
			return;
		}

		const auto nowMs = clockMs();
		std::uint64_t id = 0;
		std::int64_t startMs = 0;
		bool cleared = false;
		{
			std::scoped_lock lock{ g_phaseLock };
			if (g_phase == phase)
			{
				id = g_phaseID;
				startMs = g_phaseStartMs;
				g_phase.clear();
				g_phaseID = 0;
				g_phaseStartMs = 0;
				g_phaseLastWarnMs = 0;
				cleared = true;
			}
		}

		if (cleared)
		{
			REX::INFO(
				"{} load-watch stage=clear id={} phase='{}' elapsed={:.2f} ms thread={}.",
				Plugin::NAME,
				id,
				phase,
				static_cast<double>(nowMs - startMs),
				currentThreadHash());
		}
	}

	ScopedPhase::ScopedPhase(std::string_view phase, double logAfterMs) :
		m_phase(phase),
		m_token(BeginPhase(m_phase)),
		m_logAfterMs(logAfterMs)
	{}

	ScopedPhase::~ScopedPhase()
	{
		EndPhase(m_phase, m_token, m_logAfterMs);
	}
}
