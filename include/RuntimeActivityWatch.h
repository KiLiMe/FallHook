// AI CONTEXT: Exposes aggressive FallHook activity tracing for hook and internal work attribution.
// Depends only on standard timing-safe types; implementation owns logging and burst counters.
// Runtime assumptions: version-neutral diagnostics for whichever runtime module is active.
// Version-specific logic: none; concrete hook/work names are supplied by runtime modules.
// Source-free policy: records activity names and timing only; never logs or matches translation text.
#pragma once

#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

namespace RuntimeActivityWatch
{
	enum class ScopeKind : std::uint8_t
	{
		kHook,
		kWork
	};

	struct Settings
	{
		bool enable{ false };
		bool logBegin{ true };
		bool logEnd{ true };
		bool logFastCalls{ true };
		std::uint32_t slowUs{ 500 };
		std::uint32_t burstWindowMs{ 250 };
		std::uint32_t burstCallWarn{ 128 };
		bool includeWorkScopes{ true };
		bool includeThreadDepth{ true };
	};

	struct Token
	{
		std::string_view name;
		std::string_view parent;
		std::uint64_t sequence{ 0 };
		std::int64_t startUs{ 0 };
		std::uint32_t depth{ 0 };
		ScopeKind kind{ ScopeKind::kHook };
		bool active{ false };
	};

	void Configure(const Settings& settings);
	void Shutdown() noexcept;
	[[nodiscard]] bool Enabled() noexcept;
	[[nodiscard]] Token Begin(ScopeKind kind, std::string_view name) noexcept;
	void End(Token& token) noexcept;

	class WorkScope
	{
	public:
		explicit WorkScope(std::string_view name) noexcept;
		~WorkScope();

		WorkScope(const WorkScope&) = delete;
		WorkScope(WorkScope&&) = delete;
		WorkScope& operator=(const WorkScope&) = delete;
		WorkScope& operator=(WorkScope&&) = delete;

	private:
		Token token_;
	};

	template <class Func>
	decltype(auto) RunWork(std::string_view name, Func&& func)
	{
		WorkScope scope{ name };
		if constexpr (std::is_void_v<std::invoke_result_t<Func>>)
		{
			std::forward<Func>(func)();
		}
		else
		{
			return std::forward<Func>(func)();
		}
	}
}
