// AI CONTEXT: Records compact hook timing while the save-load gap watchdog is active.
// Depends on RuntimeLoadWatchdog for the active gap flag and shared logging for summaries.
// Runtime assumptions: version-neutral diagnostics; runtime modules supply stable hook names.
// Version-specific logic: none; this helper only records named hook timing.
// Source-free policy: logs hook names and timing only; never logs or matches text content.
#pragma once

#include <cstdint>
#include <string_view>

namespace RuntimeSaveLoadGapTrace
{
	class ScopedHook
	{
	public:
		explicit ScopedHook(std::string_view hookName) noexcept;
		~ScopedHook();

		ScopedHook(const ScopedHook&) = delete;
		ScopedHook(ScopedHook&&) = delete;
		ScopedHook& operator=(const ScopedHook&) = delete;
		ScopedHook& operator=(ScopedHook&&) = delete;

	private:
		std::string_view hookName_;
		std::int64_t startUs_{ 0 };
		bool active_{ false };
	};

	void Reset() noexcept;
	void LogSummaryAndReset(std::string_view reason);
}
