// AI CONTEXT: Installs the Pip-Boy log stat visitor hook that supplies GMST-backed labels during data populate.
// Depends on RuntimePipboyLogTranslations, RuntimeHookWatch, and the 1.11.240 PopulateStatsVisitor vtable.
// Runtime scope is Fallout 4 1.11.240 PipboyLogData::PopulateStatsVisitor only.
// Version-specific logic: Fallout 4 1.11.240 only; no alternate runtime branches.
// Source-free policy: lookup uses stat keys/editor IDs only; original label text is never a lookup key.
#include "PCH.h"

#include "RuntimeApplySettings.h"
#include "RuntimeActivityWatch.h"
#include "RuntimeHookWatch.h"
#include "RuntimeSaveLoadGapTrace.h"
#include "111240/RuntimePipboyLogHook.h"
#include "111240/RuntimePipboyLogTranslations.h"

#include <atomic>
#include <optional>

namespace Runtime111240
{
namespace
{
	using VisitFunc = bool(void*, RE::BSFixedString&, RE::BSFixedString&, std::uint32_t, int, bool);

	constexpr std::size_t kVisitorVisitSlot{ 0 };
	constexpr std::uint32_t kTraceLimit{ 96 };

	VisitFunc* g_originalVisit{ nullptr };
	RuntimeHookWatch::Stats g_visitWatch;
	std::atomic_bool g_traceEnabled{ false };
	std::atomic_uint32_t g_traceLines{ 0 };
	bool g_installed{ false };

	[[nodiscard]] std::string_view fixedStringView(const RE::BSFixedString& value)
	{
		const auto* text = value.c_str();
		return text ? std::string_view{ text, value.length() } : std::string_view{};
	}

	[[nodiscard]] std::optional<RuntimePipboyLogTranslations::LookupResult> findTranslation(
		const RE::BSFixedString& first,
		const RE::BSFixedString& second)
	{
		if (auto lookup = RuntimePipboyLogTranslations::LookupStatKey(fixedStringView(first)))
		{
			return lookup;
		}
		if (auto lookup = RuntimePipboyLogTranslations::LookupStatKey(fixedStringView(second)))
		{
			return lookup;
		}
		return std::nullopt;
	}

	void traceVisit(std::string_view stage, std::string_view resolved, std::size_t textLen)
	{
		if (!g_traceEnabled.load(std::memory_order_relaxed))
		{
			return;
		}
		if (g_traceLines.fetch_add(1, std::memory_order_relaxed) >= kTraceLimit)
		{
			return;
		}

		REX::INFO(
			"{} pipboy-log visitor trace stage={} resolved={} replace=second textLen={}",
			Plugin::NAME,
			stage,
			resolved,
			textLen);
	}

	bool visitThunk(void* visitor, RE::BSFixedString& first, RE::BSFixedString& second, std::uint32_t type, int value, bool flag)
	{
		RuntimeSaveLoadGapTrace::ScopedHook gapTrace{ "PipboyLogData::PopulateStatsVisitor::Visit" };
		RuntimeHookWatch::ScopedCall hookWatch{ g_visitWatch, "PipboyLogData::PopulateStatsVisitor::Visit" };
		const auto lookup = RuntimeActivityWatch::RunWork(
			"PipboyLog lookup",
			[&]() { return findTranslation(first, second); });
		if (!lookup || !lookup->text || lookup->text[0] == '\0')
		{
			return g_originalVisit(visitor, first, second, type, value, flag);
		}

		auto translated = RuntimeActivityWatch::RunWork(
			"PipboyLog fixed string",
			[&]() { return RE::BSFixedString{ lookup->text }; });
		traceVisit("replace", lookup->editorID, fixedStringView(translated).size());
		return g_originalVisit(visitor, first, translated, type, value, flag);
	}
}

namespace RuntimePipboyLogHook
{
	void Install()
	{
		if (g_installed)
		{
			return;
		}

		g_traceEnabled.store(RuntimeApplySettings::Load().TraceEnabled(), std::memory_order_relaxed);
		g_traceLines.store(0, std::memory_order_relaxed);

		REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE::PipboyLogData__PopulateStatsVisitor[0] };
		const auto slotAddress = vtable.address() + sizeof(std::uintptr_t) * kVisitorVisitSlot;
		g_originalVisit = reinterpret_cast<VisitFunc*>(vtable.write_vfunc(kVisitorVisitSlot, visitThunk));
		g_installed = g_originalVisit != nullptr;
		REX::INFO(
			"{} installed PipboyLogData::PopulateStatsVisitor::Visit hook at {:X} slot={}.",
			Plugin::NAME,
			slotAddress,
			kVisitorVisitSlot);
	}
}

} // namespace Runtime111240
