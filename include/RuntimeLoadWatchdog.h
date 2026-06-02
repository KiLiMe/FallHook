// AI CONTEXT: Tracks startup/save-load phases and reports slow active loading work to FallHook.log.
// Depends only on the plugin runtime target and independent [Watchdog] settings from main.
// Runtime assumptions: version-neutral timing diagnostics for active module phases.
// Version-specific logic: none; phase names come from selected modules.
// Source-free policy: phase names use module/record identity context only; no Source text lookup.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace RuntimeLoadWatchdog
{
	struct PhaseToken
	{
		std::uint64_t id{ 0 };
		std::int64_t startMs{ 0 };
	};

	void Configure(bool enabled);
	void Shutdown();
	[[nodiscard]] bool Enabled() noexcept;
	void SetSaveLoadGapActive(bool active) noexcept;
	[[nodiscard]] bool SaveLoadGapActive() noexcept;

	[[nodiscard]] PhaseToken BeginPhase(std::string_view phase);
	void EndPhase(std::string_view phase, PhaseToken token, double logAfterMs = 0.0);
	void SetWaitPhase(std::string_view phase);
	void ClearWaitPhase(std::string_view phase);

	class ScopedPhase
	{
	public:
		explicit ScopedPhase(std::string_view phase, double logAfterMs = 0.0);
		~ScopedPhase();

		ScopedPhase(const ScopedPhase&) = delete;
		ScopedPhase(ScopedPhase&&) = delete;
		ScopedPhase& operator=(const ScopedPhase&) = delete;
		ScopedPhase& operator=(ScopedPhase&&) = delete;

	private:
		std::string m_phase;
		PhaseToken m_token;
		double m_logAfterMs{ 0.0 };
	};
}
