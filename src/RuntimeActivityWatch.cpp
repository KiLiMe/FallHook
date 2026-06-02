// AI CONTEXT: Implements aggressive FallHook activity tracing for stutter attribution.
// Depends on REX logging and shared settings supplied by RuntimeWatchdogSettings/main.
// Runtime assumptions: version-neutral diagnostics; active runtime modules only supply names.
// Version-specific logic: none; no offsets, relocation IDs, or runtime layout assumptions.
// Source-free policy: logs scope identity and timing only; no game text or XML text is recorded.
#include "PCH.h"

#include "RuntimeActivityWatch.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace
{
	struct BurstBucket
	{
		std::string_view name;
		RuntimeActivityWatch::ScopeKind kind{ RuntimeActivityWatch::ScopeKind::kHook };
		std::int64_t windowStartMs{ 0 };
		std::uint64_t calls{ 0 };
		std::uint64_t totalUs{ 0 };
		std::uint64_t maxUs{ 0 };
	};

	std::atomic_bool g_enabled{ false };
	std::atomic_bool g_logBegin{ true };
	std::atomic_bool g_logEnd{ true };
	std::atomic_bool g_logFastCalls{ true };
	std::atomic_bool g_includeWorkScopes{ true };
	std::atomic_bool g_includeThreadDepth{ true };
	std::atomic_uint32_t g_slowUs{ 500 };
	std::atomic_uint32_t g_burstWindowMs{ 250 };
	std::atomic_uint32_t g_burstCallWarn{ 128 };
	std::atomic_uint64_t g_sequence{ 0 };
	std::mutex g_burstLock;
	std::array<BurstBucket, 256> g_bursts;
	thread_local std::array<std::string_view, 64> g_scopeStack;
	thread_local std::uint32_t g_scopeDepth = 0;

	[[nodiscard]] std::int64_t clockUs()
	{
		return std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now().time_since_epoch())
			.count();
	}

	[[nodiscard]] std::int64_t clockMs()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch())
			.count();
	}

	[[nodiscard]] std::uint64_t currentThreadHash()
	{
		return static_cast<std::uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
	}

	[[nodiscard]] const char* kindText(RuntimeActivityWatch::ScopeKind kind) noexcept
	{
		return kind == RuntimeActivityWatch::ScopeKind::kHook ? "hook" : "work";
	}

	[[nodiscard]] std::uint64_t avgUs(std::uint64_t totalUs, std::uint64_t calls) noexcept
	{
		return calls == 0 ? 0 : totalUs / calls;
	}

	[[nodiscard]] BurstBucket* findBurstBucket(RuntimeActivityWatch::ScopeKind kind, std::string_view name)
	{
		BurstBucket* empty = nullptr;
		for (auto& bucket : g_bursts)
		{
			if (bucket.name.empty())
			{
				empty = empty ? empty : std::addressof(bucket);
				continue;
			}
			if (bucket.kind == kind && bucket.name == name)
			{
				return std::addressof(bucket);
			}
		}
		return empty;
	}

	void logBegin(const RuntimeActivityWatch::Token& token)
	{
		if (!g_logBegin.load(std::memory_order_relaxed) || !g_logFastCalls.load(std::memory_order_relaxed))
		{
			return;
		}

		REX::INFO(
			"{} activity stage=begin kind={} seq={} depth={} parent='{}' name='{}' thread={}.",
			Plugin::NAME,
			kindText(token.kind),
			token.sequence,
			g_includeThreadDepth.load(std::memory_order_relaxed) ? token.depth : 0,
			token.parent,
			token.name,
			currentThreadHash());
	}

	void logEnd(const RuntimeActivityWatch::Token& token, std::uint64_t elapsedUs)
	{
		if (!g_logEnd.load(std::memory_order_relaxed))
		{
			return;
		}
		if (!g_logFastCalls.load(std::memory_order_relaxed) &&
			elapsedUs < g_slowUs.load(std::memory_order_relaxed))
		{
			return;
		}

		REX::INFO(
			"{} activity stage=end kind={} seq={} depth={} parent='{}' name='{}' elapsedUs={} elapsedMs={} thread={}.",
			Plugin::NAME,
			kindText(token.kind),
			token.sequence,
			g_includeThreadDepth.load(std::memory_order_relaxed) ? token.depth : 0,
			token.parent,
			token.name,
			elapsedUs,
			(elapsedUs + 999u) / 1000u,
			currentThreadHash());
	}

	void flushBurst(BurstBucket& bucket, std::int64_t nowMs, std::uint32_t windowMs, std::uint32_t warnCalls)
	{
		if (bucket.windowStartMs == 0)
		{
			bucket.windowStartMs = nowMs;
			return;
		}
		const auto elapsedMs = nowMs - bucket.windowStartMs;
		if (elapsedMs < windowMs)
		{
			return;
		}

		if (bucket.calls >= warnCalls)
		{
			REX::WARN(
				"{} activity stage=burst kind={} name='{}' calls={} totalUs={} avgUs={} maxUs={} windowMs={} thread={}.",
				Plugin::NAME,
				kindText(bucket.kind),
				bucket.name,
				bucket.calls,
				bucket.totalUs,
				avgUs(bucket.totalUs, bucket.calls),
				bucket.maxUs,
				elapsedMs,
				currentThreadHash());
		}

		bucket.windowStartMs = nowMs;
		bucket.calls = 0;
		bucket.totalUs = 0;
		bucket.maxUs = 0;
	}

	void recordBurst(RuntimeActivityWatch::ScopeKind kind, std::string_view name, std::uint64_t elapsedUs)
	{
		const auto windowMs = g_burstWindowMs.load(std::memory_order_relaxed);
		const auto warnCalls = g_burstCallWarn.load(std::memory_order_relaxed);
		if (windowMs == 0 || warnCalls == 0)
		{
			return;
		}

		const auto nowMs = clockMs();
		std::scoped_lock lock{ g_burstLock };
		auto* bucket = findBurstBucket(kind, name);
		if (!bucket)
		{
			return;
		}
		if (bucket->name.empty())
		{
			bucket->name = name;
			bucket->kind = kind;
			bucket->windowStartMs = nowMs;
		}
		++bucket->calls;
		bucket->totalUs += elapsedUs;
		bucket->maxUs = std::max(bucket->maxUs, elapsedUs);
		flushBurst(*bucket, nowMs, windowMs, warnCalls);
	}
}

namespace RuntimeActivityWatch
{
	void Configure(const Settings& settings)
	{
		g_enabled.store(settings.enable, std::memory_order_release);
		g_logBegin.store(settings.logBegin, std::memory_order_release);
		g_logEnd.store(settings.logEnd, std::memory_order_release);
		g_logFastCalls.store(settings.logFastCalls, std::memory_order_release);
		g_includeWorkScopes.store(settings.includeWorkScopes, std::memory_order_release);
		g_includeThreadDepth.store(settings.includeThreadDepth, std::memory_order_release);
		g_slowUs.store(settings.slowUs, std::memory_order_release);
		g_burstWindowMs.store(settings.burstWindowMs, std::memory_order_release);
		g_burstCallWarn.store(settings.burstCallWarn, std::memory_order_release);
		if (settings.enable)
		{
			REX::INFO(
				"{} activity watchdog active: logBegin={} logEnd={} logFastCalls={} slowUs={} burstWindowMs={} burstCallWarn={} includeWorkScopes={}.",
				Plugin::NAME,
				settings.logBegin,
				settings.logEnd,
				settings.logFastCalls,
				settings.slowUs,
				settings.burstWindowMs,
				settings.burstCallWarn,
				settings.includeWorkScopes);
		}
	}

	void Shutdown() noexcept
	{
		g_enabled.store(false, std::memory_order_release);
	}

	bool Enabled() noexcept
	{
		return g_enabled.load(std::memory_order_acquire);
	}

	Token Begin(ScopeKind kind, std::string_view name) noexcept
	{
		Token token{ .name = name, .kind = kind };
		if (!Enabled() || name.empty())
		{
			return token;
		}
		if (kind == ScopeKind::kWork && !g_includeWorkScopes.load(std::memory_order_acquire))
		{
			return token;
		}

		token.active = true;
		token.sequence = g_sequence.fetch_add(1, std::memory_order_acq_rel) + 1;
		token.depth = g_scopeDepth + 1;
		token.parent = g_scopeDepth == 0 ? std::string_view{} :
			g_scopeStack[std::min<std::uint32_t>(g_scopeDepth, static_cast<std::uint32_t>(g_scopeStack.size())) - 1];
		if (g_scopeDepth < g_scopeStack.size())
		{
			g_scopeStack[g_scopeDepth] = name;
		}
		++g_scopeDepth;
		logBegin(token);
		token.startUs = clockUs();
		return token;
	}

	void End(Token& token) noexcept
	{
		if (!token.active)
		{
			return;
		}

		const auto elapsedRaw = clockUs() - token.startUs;
		const auto elapsedUs = elapsedRaw <= 0 ? 0u : static_cast<std::uint64_t>(elapsedRaw);
		if (g_scopeDepth > 0)
		{
			--g_scopeDepth;
		}
		recordBurst(token.kind, token.name, elapsedUs);
		logEnd(token, elapsedUs);
		token.active = false;
	}

	WorkScope::WorkScope(std::string_view name) noexcept :
		token_(Begin(ScopeKind::kWork, name))
	{}

	WorkScope::~WorkScope()
	{
		End(token_);
	}
}
