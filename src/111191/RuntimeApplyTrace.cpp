// AI CONTEXT: Emits gated source-free const-apply trace entries into FallHook.log.
// Depends on RuntimeApplySettings gates, ConstApplyEntry identity data, and CommonLibF4 forms.
// Runtime scope is Fallout 4 1.11.191 runtime apply diagnostics.
// Version-specific logic: Fallout 4 1.11.191 only; no alternate runtime branches.
// Source-free policy: trace output names plugin/form/editor/slot identity only, never Source text.
#include "PCH.h"

#include "ConstApplyMap.h"
#include "111191/RuntimeApplyTrace.h"

#include <atomic>

namespace Runtime111191
{
namespace
{
	constexpr std::uint32_t kTraceLineLimit = 4096;

	std::atomic_uint32_t g_attemptedLines{ 0 };
	std::atomic_uint32_t g_suppressedLines{ 0 };

	[[nodiscard]] bool shouldWriteTrace()
	{
		const auto previous = g_attemptedLines.fetch_add(1, std::memory_order_relaxed);
		if (previous < kTraceLineLimit)
		{
			return true;
		}

		const auto suppressed = g_suppressedLines.fetch_add(1, std::memory_order_relaxed) + 1;
		if (previous == kTraceLineLimit)
		{
			REX::INFO(
				"{} const trace stage=limit-reached cap={} suppressed={}",
				Plugin::NAME,
				kTraceLineLimit,
				suppressed);
		}
		return false;
	}
}

namespace RuntimeApplyTrace
{
	void Reset()
	{
		g_attemptedLines.store(0, std::memory_order_relaxed);
		g_suppressedLines.store(0, std::memory_order_relaxed);
	}

	void Summary(const RuntimeApplySettings::Values& settings, std::string_view reason)
	{
		if (!settings.TraceEnabled())
		{
			return;
		}

		const auto attempted = g_attemptedLines.load(std::memory_order_relaxed);
		const auto suppressed = g_suppressedLines.load(std::memory_order_relaxed);
		REX::INFO(
			"{} const trace summary reason={} attempted={} logged={} suppressed={} cap={}",
			Plugin::NAME,
			reason,
			attempted,
			std::min(attempted, kTraceLineLimit),
			suppressed,
			kTraceLineLimit);
	}

	void Entry(
		const RuntimeApplySettings::Values& settings,
		std::string_view stage,
		std::string_view target,
		std::uint32_t rawFormID,
		const ConstApplyEntry& entry,
		const RE::TESForm* form)
	{
		if (!settings.TraceEnabled() || !shouldWriteTrace())
		{
			return;
		}

		REX::INFO(
			"{} const trace stage={} target={} plugin={} rec={} type={} raw={:08X} resolved={:08X} formType={} editor={} index={} sid={} textLen={}",
			Plugin::NAME,
			stage,
			target,
			entry.pluginName,
			entry.recordSignature,
			static_cast<std::uint8_t>(entry.data.translationType),
			rawFormID,
			form ? form->formID : 0,
			form ? form->GetFormTypeString() : "",
			entry.data.editorID.value_or(""),
			entry.data.index.value_or(0xFFFFFFFFu),
			entry.data.stringID.value_or(0xFFFFFFFFu),
			entry.data.replacerText.size());
	}
}

} // namespace Runtime111191
