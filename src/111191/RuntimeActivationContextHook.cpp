// AI CONTEXT: Captures the current activation reference/base identity for activation lookups.
// Depends on RuntimeActivationLookContext, RuntimeApplySettings, and prologue hooks.
// Runtime scope is Fallout 4 1.11.191 TESObjectREFR::GetActivateText.
// Version-specific logic: fixed Address Library ID 2201128 for Fallout 4 1.11.191.
// Source-free policy: stores form and editorID identity; live text is diagnostic only.
#include "PCH.h"

#include "111191/RuntimeActivationContextHook.h"

#include "111191/RuntimeActivationLookContext.h"
#include "111191/RuntimeActivationTextTranslations.h"
#include "RuntimeApplySettings.h"
#include "RuntimeHookWatch.h"
#include "RuntimePrologueHook.h"

#include "RE/E/ENUM_FORM_ID.h"
#include "RE/T/TESObjectREFR.h"

#include <atomic>

namespace Runtime111191
{
namespace
{
	using GetActivateTextFunc = bool(RE::TESObjectREFR*, RE::BSString&);

	constexpr REL::ID kGetActivateTextID{ 2201128 };
	constexpr std::size_t kMaxPatchBytes{ 16 };
	constexpr std::uint32_t kTraceLimit{ 256 };

	GetActivateTextFunc* g_originalGetActivateText{ nullptr };
	RuntimeHookWatch::Stats g_hookWatch;
	RuntimeHookWatch::Stats g_hookWorkWatch;
	std::atomic_bool g_traceEnabled{ false };
	std::atomic_uint32_t g_traceLines{ 0 };
	bool g_installed{ false };

	[[nodiscard]] std::uint32_t formID(const RE::TESForm* form) noexcept
	{
		return form ? static_cast<std::uint32_t>(form->formID) : 0;
	}

	[[nodiscard]] std::uint32_t formLocalID(const RE::TESForm* form) noexcept
	{
		return formID(form) & 0x00FFFFFFu;
	}

	[[nodiscard]] std::uint32_t formTypeID(const RE::TESForm* form) noexcept
	{
		return form ? static_cast<std::uint32_t>(form->GetFormType()) : 0;
	}

	[[nodiscard]] const char* formTypeString(std::uint32_t typeID) noexcept
	{
		if (typeID == 0 || typeID >= static_cast<std::uint32_t>(RE::ENUM_FORM_ID::kTotal))
		{
			return "";
		}
		return RE::TESForm::GetFormTypeString(static_cast<RE::ENUM_FORM_ID>(typeID));
	}

	[[nodiscard]] const char* editorID(const RE::TESForm* form) noexcept
	{
		if (!form)
		{
			return "";
		}
		const auto* editor = form->GetFormEditorID();
		return editor ? editor : "";
	}

	[[nodiscard]] std::string textSnippet(const RE::BSString& text)
	{
		const auto* raw = text.c_str();
		if (!raw || text.length() == 0)
		{
			return {};
		}

		constexpr std::size_t kLimit{ 96 };
		std::string snippet{ std::string_view{ raw, text.length() }.substr(0, kLimit) };
		for (auto& ch : snippet)
		{
			if (ch == '\r' || ch == '\n' || ch == '\t')
			{
				ch = ' ';
			}
		}
		if (text.length() > kLimit)
		{
			snippet += "...";
		}
		return snippet;
	}

	void traceContext(
		const RE::TESObjectREFR* reference,
		const RE::TESForm* base,
		const RE::BSString& text,
		bool result)
	{
		const auto traceEnabled = g_traceEnabled.load(std::memory_order_relaxed);
		const auto refTypeID = formTypeID(reference);
		const auto baseTypeID = formTypeID(base);
		RuntimeActivationLookContext::Update(
			formID(reference),
			refTypeID,
			traceEnabled ? formTypeString(refTypeID) : "",
			editorID(reference),
			formID(base),
			formLocalID(base),
			baseTypeID,
			traceEnabled ? formTypeString(baseTypeID) : "",
			editorID(base),
			result,
			traceEnabled ? textSnippet(text) : "");

		if (!traceEnabled || g_traceLines.fetch_add(1, std::memory_order_relaxed) >= kTraceLimit)
		{
			return;
		}

		const auto baseAction = base ? RuntimeActivationTextTranslations::Lookup(base, std::nullopt) : std::nullopt;
		const auto refAction = reference ? RuntimeActivationTextTranslations::Lookup(reference, std::nullopt) : std::nullopt;

		REX::INFO(
			"{} activation-context trace ref={:08X} refType={} refEditor={} base={:08X} baseLocal={:06X} baseType={} baseEditor={} result={} textLen={} baseActionLen={} refActionLen={}",
			Plugin::NAME,
			formID(reference),
			formTypeString(refTypeID),
			editorID(reference),
			formID(base),
			formLocalID(base),
			formTypeString(baseTypeID),
			editorID(base),
			result,
			text.length(),
			baseAction ? baseAction->size() : 0,
			refAction ? refAction->size() : 0);
	}

	bool getActivateTextThunk(RE::TESObjectREFR* reference, RE::BSString& out)
	{
		RuntimeHookWatch::ScopedCall hookWatch{ g_hookWatch, "TESObjectREFR::GetActivateText" };
		const auto result = g_originalGetActivateText(reference, out);
		RuntimeHookWatch::ScopedCall workWatch{ g_hookWorkWatch, "TESObjectREFR::GetActivateText FallHook work" };
		const auto* base = reference ? reference->GetObjectReference() : nullptr;
		traceContext(reference, base, out, result);
		return result;
	}

	void installHook()
	{
		REL::Relocation<std::uintptr_t> target{ kGetActivateTextID };
		const auto result = RuntimePrologueHook::InstallJump(
			target.address(),
			reinterpret_cast<std::uintptr_t>(getActivateTextThunk),
			kMaxPatchBytes);
		if (!result.installed)
		{
			REX::ERROR("{} skipped TESObjectREFR::GetActivateText context hook; unsupported prologue bytes: {}", Plugin::NAME, result.prologueBytes);
			return;
		}

		g_originalGetActivateText = reinterpret_cast<GetActivateTextFunc*>(result.original);
		REX::INFO("{} installed TESObjectREFR::GetActivateText context hook at {:X} using Address Library ID {}.", Plugin::NAME, target.address(), kGetActivateTextID.id());
	}
}

namespace RuntimeActivationContextHook
{
	void Install()
	{
		if (g_installed)
		{
			return;
		}

		g_traceEnabled.store(RuntimeApplySettings::Load().TraceEnabled(), std::memory_order_relaxed);
		g_traceLines.store(0, std::memory_order_relaxed);
		installHook();
		g_installed = g_originalGetActivateText != nullptr;
	}
}

} // namespace Runtime111191
