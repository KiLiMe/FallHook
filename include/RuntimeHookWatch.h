// AI CONTEXT: Exposes low-overhead per-hook timing diagnostics for suspicious runtime stalls.
// Depends only on atomics and is configured by main from independent [Watchdog] settings.
// Runtime assumptions: version-neutral diagnostics for whichever module is active.
// Version-specific logic: none; concrete hook files own relocation IDs and offsets.
// Source-free policy: records hook names and timings only; never uses Source text for identity.
#pragma once

#include "RuntimeActivityWatch.h"

#include <atomic>
#include <cstdint>
#include <string_view>

namespace RuntimeHookWatch
{
	struct Stats
	{
		std::atomic_uint64_t calls{ 0 };
		std::atomic_uint64_t slowCalls{ 0 };
		std::atomic_uint64_t totalUs{ 0 };
		std::atomic_uint64_t activeSequence{ 0 };
		std::atomic_uint64_t activeThread{ 0 };
		std::atomic_int64_t activeSinceMs{ 0 };
		std::atomic_int64_t lastSlowLogMs{ 0 };
		std::atomic_uint32_t maxMs{ 0 };
		std::atomic_uint32_t slowLogs{ 0 };
		std::atomic_bool activeReported{ false };
		std::atomic_bool registered{ false };
	};

	void Configure(bool enabled) noexcept;
	void Shutdown();
	[[nodiscard]] bool Enabled() noexcept;

	class ScopedCall
	{
	public:
		ScopedCall(Stats& stats, std::string_view hookName) noexcept;
		~ScopedCall();

		ScopedCall(const ScopedCall&) = delete;
		ScopedCall(ScopedCall&&) = delete;
		ScopedCall& operator=(const ScopedCall&) = delete;
		ScopedCall& operator=(ScopedCall&&) = delete;

	private:
		Stats* stats_{ nullptr };
		std::string_view hookName_;
		RuntimeActivityWatch::Token activity_;
		std::uint64_t activeSequence_{ 0 };
		std::int64_t startMs_{ 0 };
		std::int64_t startUs_{ 0 };
		bool active_{ false };
	};
}
