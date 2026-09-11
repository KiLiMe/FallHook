// AI CONTEXT: Hooks TESDescription::GetDescription and applies source-free DESC translations.
// Depends on MessageIconFormatter, RuntimeDescriptionTranslations, RuntimeApplySettings, RuntimeMemorySafety, and RuntimePrologueHook.
// Runtime scope is Fallout 4 1.11.240 Address Library ID 2193019 only.
// Version-specific logic: Fallout 4 1.11.240 only; no alternate runtime branches.
// Source-free policy: original description output is never a lookup key; it only donates icon markup after lookup.
// Exception note: MESG/DESC icon preservation is approved and must not be treated as source-exact lookup.
#include "PCH.h"

#include "MessageIconFormatter.h"
#include "RuntimeApplySettings.h"
#include "RuntimeActivityWatch.h"
#include "111240/RuntimeDescriptionHook.h"
#include "111240/RuntimeDescriptionTranslations.h"
#include "RuntimeHookWatch.h"
#include "RuntimeMemorySafety.h"
#include "RuntimePrologueHook.h"
#include "RuntimeSaveLoadGapTrace.h"

#include <atomic>

namespace Runtime111240
{
namespace
{
	using GetDescriptionFunc = void(RE::TESDescription*, RE::BSString&, const RE::TESForm*);

	constexpr REL::ID kGetDescriptionID{ 2193019 };
	constexpr std::size_t kMaxPatchBytes{ 16 };
	constexpr std::uint32_t kTraceLimit{ 256 };

	GetDescriptionFunc* g_originalGetDescription{ nullptr };
	RuntimeHookWatch::Stats g_hookWatch;
	std::atomic_bool g_traceEnabled{ false };
	std::atomic_uint32_t g_traceLines{ 0 };
	bool g_installed{ false };

	[[nodiscard]] std::uint32_t safeFormID(const RE::TESForm* form)
	{
		if (!RuntimeMemorySafety::IsReadableMemory(form, sizeof(RE::TESForm)))
		{
			return 0;
		}
		return form->formID;
	}

	[[nodiscard]] std::uint32_t safeFormType(const RE::TESForm* form)
	{
		if (!RuntimeMemorySafety::IsReadableMemory(form, sizeof(RE::TESForm)))
		{
			return 0;
		}
		return std::to_underlying(form->GetFormType());
	}

	void trace(std::string_view stage, const RE::TESDescription* description, const RE::TESForm* form, std::uint32_t formID, std::size_t textLen, bool owner)
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
			"{} description trace stage={} form={:08X} formType={} description={:016X} textLen={} ownerIndex={}",
			Plugin::NAME,
			stage,
			formID ? formID : safeFormID(form),
			safeFormType(form),
			reinterpret_cast<std::uintptr_t>(description),
			textLen,
			owner);
	}

	void getDescriptionThunk(RE::TESDescription* description, RE::BSString& out, const RE::TESForm* form)
	{
		RuntimeSaveLoadGapTrace::ScopedHook gapTrace{ "TESDescription::GetDescription" };
		RuntimeHookWatch::ScopedCall hookWatch{ g_hookWatch, "TESDescription::GetDescription" };
		g_originalGetDescription(description, out, form);

		const auto lookup = RuntimeActivityWatch::RunWork(
			"Description lookup",
			[&]() { return RuntimeDescriptionTranslations::Lookup(description, form); });
		if (!lookup)
		{
			trace("miss", description, form, 0, 0, false);
			return;
		}

		const auto* runtimeText = out.c_str();
		const auto runtimeTextView = runtimeText ? std::string_view{ runtimeText, out.length() } : std::string_view{};
		const auto formatted = RuntimeActivityWatch::RunWork(
			"Description icon format",
			[&]() { return MessageIconFormatter::ApplyRuntimeControlMarkup(runtimeTextView, lookup->text); });
		RuntimeActivityWatch::RunWork("Description assign", [&]() { out.Set(formatted.text.c_str(), formatted.text.size() + 1); });
		trace(formatted.changed() ? "hit-icon-format" : "hit", description, form, lookup->formID, formatted.text.size(), lookup->usedOwnerIndex);
	}
}

namespace RuntimeDescriptionHook
{
	void Install()
	{
		if (g_installed)
		{
			return;
		}

		g_traceEnabled.store(RuntimeApplySettings::Load().TraceEnabled(), std::memory_order_relaxed);
		g_traceLines.store(0, std::memory_order_relaxed);

		REL::Relocation<std::uintptr_t> target{ kGetDescriptionID };
		const auto result = RuntimePrologueHook::InstallJump(
			target.address(),
			reinterpret_cast<std::uintptr_t>(getDescriptionThunk),
			kMaxPatchBytes);
		if (!result.installed)
		{
			REX::ERROR(
				"{} skipped TESDescription::GetDescription hook; unsupported prologue bytes: {}",
				Plugin::NAME,
				result.prologueBytes);
			return;
		}

		g_originalGetDescription = reinterpret_cast<GetDescriptionFunc*>(result.original);
		g_installed = true;
		REX::INFO(
			"{} installed TESDescription::GetDescription hook at {:X} using Address Library ID {}.",
			Plugin::NAME,
			target.address(),
			kGetDescriptionID.id());
	}
}

} // namespace Runtime111240
