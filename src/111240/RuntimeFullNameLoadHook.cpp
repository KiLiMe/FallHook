// AI CONTEXT: Hooks TESFullName::LoadFullNameChunk and mutates loaded FULL data immediately.
// Depends on RuntimeCatalogProvider, RuntimeFullNameLoadTranslations, RuntimeTextStringAssign, and prologue hooks.
// Runtime scope is Fallout 4 1.11.240 Address Library ID 2193215 only.
// Version-specific logic: the hook target is verified for Fallout 4 1.11.240 only.
// Source-free policy: lookup uses loaded FormID/editorID only; original FULL text is diagnostic-only.
#include "PCH.h"

#include "RuntimeApplySettings.h"
#include "RuntimeActivityWatch.h"
#include "111240/RuntimeCatalogProvider.h"
#include "111240/RuntimeFullNameLoadHook.h"
#include "111240/RuntimeFullNameOwnerResolver.h"
#include "111240/RuntimeFullNameLoadTranslations.h"
#include "RuntimeHookWatch.h"
#include "111240/RuntimeLocalizedStringID.h"
#include "RuntimePrologueHook.h"
#include "RuntimeSaveLoadGapTrace.h"
#include "111240/RuntimeTextStringAssign.h"

#include <atomic>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>

namespace Runtime111240
{
namespace
{
	using LoadFullNameChunkFunc = void(RE::TESFullName*, RE::TESFile*);

	constexpr REL::ID kLoadFullNameChunkID{ 2193215 };
	constexpr std::size_t kMaxPatchBytes{ 16 };
	constexpr std::uint32_t kTraceLimit{ 256 };
	constexpr std::uint32_t kNpcTraceLimit{ 128 };
	constexpr std::uint64_t kStatsLogEvery{ 8192 };

	struct FullNameHookStats
	{
		std::atomic_uint64_t calls{ 0 };
		std::atomic_uint64_t unreadable{ 0 };
		std::atomic_uint64_t sidSkips{ 0 };
		std::atomic_uint64_t ownerAttempts{ 0 };
		std::atomic_uint64_t ownerMisses{ 0 };
		std::atomic_uint64_t nonTargets{ 0 };
		std::atomic_uint64_t lookupMisses{ 0 };
		std::atomic_uint64_t applied{ 0 };
		std::atomic_uint64_t npcApplied{ 0 };
		std::atomic_uint64_t fallbackApplied{ 0 };
	};

	LoadFullNameChunkFunc* g_originalLoadFullNameChunk{ nullptr };
	RuntimeHookWatch::Stats g_hookWatch;
	RuntimeHookWatch::Stats g_hookWorkWatch;
	FullNameHookStats g_stats;
	std::mutex g_catalogLock;
	std::atomic_bool g_catalogReady{ false };
	std::atomic_bool g_traceEnabled{ false };
	std::atomic_uint32_t g_traceLines{ 0 };
	std::atomic_uint32_t g_npcTraceLines{ 0 };
	bool g_installed{ false };

	[[nodiscard]] bool isPreDisplayFullNameTarget(RE::ENUM_FORM_ID type) noexcept
	{
		return type == RE::ENUM_FORM_ID::kWEAP ||
			type == RE::ENUM_FORM_ID::kARMO ||
			type == RE::ENUM_FORM_ID::kOMOD ||
			type == RE::ENUM_FORM_ID::kMISC ||
			type == RE::ENUM_FORM_ID::kNPC_;
	}

	template <class Func>
	decltype(auto) fullNameWork(std::string_view name, Func&& func)
	{
		if (RuntimeActivityWatch::Enabled())
		{
			return RuntimeActivityWatch::RunWork(name, std::forward<Func>(func));
		}

		if constexpr (std::is_void_v<std::invoke_result_t<Func>>)
		{
			std::forward<Func>(func)();
		}
		else
		{
			return std::forward<Func>(func)();
		}
	}

	void maybeLogStats(std::uint64_t calls)
	{
		if (calls == 0 || calls % kStatsLogEvery != 0)
		{
			return;
		}

		REX::INFO(
			"{} full-name-load perf: calls={} unreadable={} sidSkips={} ownerAttempts={} ownerMisses={} nonTargets={} lookupMisses={} applied={} npcApplied={} fallbackApplied={}.",
			Plugin::NAME,
			calls,
			g_stats.unreadable.load(std::memory_order_relaxed),
			g_stats.sidSkips.load(std::memory_order_relaxed),
			g_stats.ownerAttempts.load(std::memory_order_relaxed),
			g_stats.ownerMisses.load(std::memory_order_relaxed),
			g_stats.nonTargets.load(std::memory_order_relaxed),
			g_stats.lookupMisses.load(std::memory_order_relaxed),
			g_stats.applied.load(std::memory_order_relaxed),
			g_stats.npcApplied.load(std::memory_order_relaxed),
			g_stats.fallbackApplied.load(std::memory_order_relaxed));
	}

	void ensureTranslations()
	{
		if (g_catalogReady.load(std::memory_order_acquire) || !RuntimeCatalogProvider::HasLoadedPluginList())
		{
			return;
		}

		RuntimeActivityWatch::WorkScope activity{ "FullName ensure translations" };
		std::scoped_lock lock{ g_catalogLock };
		if (g_catalogReady.load(std::memory_order_relaxed))
		{
			return;
		}

		const auto* catalogResult = RuntimeCatalogProvider::Ensure("RuntimeFullNameLoadHook build translation map");
		if (!catalogResult)
		{
			REX::WARN("{} full-name-load hook could not build catalog yet.", Plugin::NAME);
			return;
		}

		const auto stats = RuntimeFullNameLoadTranslations::Rebuild(catalogResult->prepared.fullNameLoad);
		g_catalogReady.store(true, std::memory_order_release);
		if (RuntimeApplySettings::Load().TraceEnabled())
		{
			const auto ownerStats = RuntimeFullNameOwnerResolver::SnapshotStats();
			REX::INFO(
				"{} full-name-load hook summary: accepted={} unsupportedSignature={} ownerPageCacheHits={} ownerPageCacheMisses={}.",
				Plugin::NAME,
				stats.accepted,
				stats.skippedUnsupportedSignature,
				ownerStats.pageHits,
				ownerStats.pageMisses);
		}
	}

	void trace(
		std::string_view stage,
		const RE::TESForm* form,
		std::optional<std::uint32_t> stringID,
		std::optional<std::uint32_t> index,
		std::size_t textLen)
	{
		if (!g_traceEnabled.load(std::memory_order_relaxed))
		{
			return;
		}
		const auto isNpcTrace = form && form->GetFormType() == RE::ENUM_FORM_ID::kNPC_;
		if (isNpcTrace && stage == "hit-npc"sv)
		{
			return;
		}
		auto& traceCounter = isNpcTrace ? g_npcTraceLines : g_traceLines;
		const auto traceLimit = isNpcTrace ? kNpcTraceLimit : kTraceLimit;
		if (traceCounter.fetch_add(1, std::memory_order_relaxed) >= traceLimit)
		{
			return;
		}

		REX::INFO(
			"{} full-name-load trace stage={} form={:08X} formType={} editor={} sid={:08X} index={} textLen={}",
			Plugin::NAME,
			stage,
			form ? form->formID : 0,
			form ? std::to_underlying(form->GetFormType()) : 0,
			form && form->GetFormEditorID() ? form->GetFormEditorID() : "",
			stringID.value_or(0),
			index.value_or(0xFFFFFFFFu),
			textLen);
	}

	void applyLoadedFullName(RE::TESFullName* fullName)
	{
		if (!RuntimeFullNameOwnerResolver::IsReadable(fullName, sizeof(RE::TESFullName)))
		{
			g_stats.unreadable.fetch_add(1, std::memory_order_relaxed);
			return;
		}

		const auto stringID = fullNameWork(
			"FullName stringID read",
			[&]() { return RuntimeLocalizedStringID::Read(fullName->fullName); });
		if (!RuntimeFullNameLoadTranslations::MayNeedOwnerLookup(stringID))
		{
			g_stats.sidSkips.fetch_add(1, std::memory_order_relaxed);
			return;
		}

		g_stats.ownerAttempts.fetch_add(1, std::memory_order_relaxed);
		auto* form = fullNameWork(
			"FullName owner resolve",
			[&]() { return RuntimeFullNameOwnerResolver::ResolveKnownReadable(fullName); });
		if (!form)
		{
			g_stats.ownerMisses.fetch_add(1, std::memory_order_relaxed);
			return;
		}
		if (!isPreDisplayFullNameTarget(form->GetFormType()))
		{
			g_stats.nonTargets.fetch_add(1, std::memory_order_relaxed);
			return;
		}

		const auto isNpc = form->GetFormType() == RE::ENUM_FORM_ID::kNPC_;
		const auto lookup = fullNameWork(
			"FullName lookup",
			[&]() { return RuntimeFullNameLoadTranslations::Lookup(form, stringID); });
		if (lookup.text)
		{
			fullNameWork(
				"FullName assign",
				[&]() { RuntimeTextStringAssign::AssignPlainLocalized(fullName->fullName, *lookup.text); });
			g_stats.applied.fetch_add(1, std::memory_order_relaxed);
			if (isNpc)
			{
				g_stats.npcApplied.fetch_add(1, std::memory_order_relaxed);
				form->AddChange(RE::CHANGE_TYPES::kActorBaseFullName);
			}
			if (lookup.source != RuntimeFullNameLoadTranslations::LookupSource::kStringID)
			{
				g_stats.fallbackApplied.fetch_add(1, std::memory_order_relaxed);
			}

			switch (lookup.source)
			{
			case RuntimeFullNameLoadTranslations::LookupSource::kStringID:
				trace(isNpc ? "hit-npc-sid" : "hit-sid", form, stringID, std::nullopt, lookup.text->size());
				break;
			case RuntimeFullNameLoadTranslations::LookupSource::kNpcIndexedFallback:
				trace("hit-npc-indexed-fallback", form, stringID, lookup.index, lookup.text->size());
				break;
			default:
				trace(isNpc ? "hit-npc" : "hit", form, stringID, std::nullopt, lookup.text->size());
				break;
			}
			return;
		}

		g_stats.lookupMisses.fetch_add(1, std::memory_order_relaxed);
		if (isNpc)
		{
			trace("miss-npc", form, stringID, std::nullopt, 0);
			return;
		}

		trace("miss", form, stringID, std::nullopt, 0);
	}

	void loadFullNameChunkThunk(RE::TESFullName* fullName, RE::TESFile* file)
	{
		const auto calls = g_stats.calls.fetch_add(1, std::memory_order_relaxed) + 1;
		RuntimeSaveLoadGapTrace::ScopedHook gapTrace{ "TESFullName::LoadFullNameChunk" };
		RuntimeHookWatch::ScopedCall hookWatch{ g_hookWatch, "TESFullName::LoadFullNameChunk" };
		g_originalLoadFullNameChunk(fullName, file);
		RuntimeHookWatch::ScopedCall workWatch{ g_hookWorkWatch, "TESFullName::LoadFullNameChunk FallHook work" };
		ensureTranslations();
		applyLoadedFullName(fullName);
		maybeLogStats(calls);
	}
}

namespace RuntimeFullNameLoadHook
{
	void Install()
	{
		if (g_installed)
		{
			return;
		}

		g_traceEnabled.store(RuntimeApplySettings::Load().TraceEnabled(), std::memory_order_relaxed);
		g_traceLines.store(0, std::memory_order_relaxed);
		g_npcTraceLines.store(0, std::memory_order_relaxed);

		REL::Relocation<std::uintptr_t> target{ kLoadFullNameChunkID };
		const auto result = RuntimePrologueHook::InstallJump(
			target.address(),
			reinterpret_cast<std::uintptr_t>(loadFullNameChunkThunk),
			kMaxPatchBytes);
		if (!result.installed)
		{
			REX::ERROR(
				"{} skipped TESFullName::LoadFullNameChunk hook; unsupported prologue bytes: {}",
				Plugin::NAME,
				result.prologueBytes);
			return;
		}

		g_originalLoadFullNameChunk = reinterpret_cast<LoadFullNameChunkFunc*>(result.original);
		g_installed = true;
		REX::INFO(
			"{} installed TESFullName::LoadFullNameChunk data hook at {:X} using Address Library ID {}.",
			Plugin::NAME,
			target.address(),
			kLoadFullNameChunkID.id());
	}
}

} // namespace Runtime111240
