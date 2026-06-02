// AI CONTEXT: Implements HUDRollover hooks for approved action replacement and diagnostics.
// Depends on action patching, activation look context, debug settings, and prologue hooks.
// Runtime scope is the user-approved Fallout 4 1.10.163 HUDRollover post-data action exception.
// Version-specific logic: fixed HUDRollover ShowRollover ID and 1.10.163 button RVAs.
// Source-free policy: visible text is diagnostic only; approval permits no Source-based matching.
#include "PCH.h"

#include "110163/RuntimeHudRolloverHook.h"

#include "110163/RuntimeActivationLookContext.h"
#include "RuntimeApplySettings.h"
#include "RuntimeHookWatch.h"
#include "110163/RuntimeHudRolloverActionPatch.h"
#include "RuntimeInGameTextDictionary.h"
#include "RuntimeInGameTextLog.h"
#include "RuntimeMemorySafety.h"
#include "RuntimePrologueHook.h"

#include "RE/B/BSStringPool.h"
#include "RE/T/TESForm.h"

#include <Windows.h>

#ifdef ERROR
#	undef ERROR
#endif

#include <atomic>
#include <mutex>
#include <unordered_set>

namespace
{
	using ShowRolloverFunc = void(void*, void*);
	using ShowButtonFunc = void(void*, char*);

	constexpr REL::ID kShowRolloverID{ 414044 };
	constexpr REL::Offset kShowActivateButtonOffset{ 0xAB1CA0 };
	constexpr REL::Offset kShowSecondaryButtonOffset{ 0xAB1D20 };
	constexpr REL::Offset kShowRolloverNounOffset{ 0xAB1E80 };
	constexpr std::size_t kMaxPatchBytes{ 16 };
	constexpr std::size_t kFormScanBytes{ 0x80 };
	constexpr std::size_t kMaxTextBytes{ 192 };
	constexpr std::size_t kMaxWideChars{ 96 };
	constexpr std::uint32_t kTraceLineLimit{ 512 };

	struct Slot
	{
		std::size_t offset;
		std::string_view name;
	};

	constexpr std::array<Slot, 4> kRolloverTextSlots{
		Slot{ 0x00, "+00" },
		Slot{ 0x08, "+08" },
		Slot{ 0x10, "+10" },
		Slot{ 0x18, "+18" }
	};

	ShowRolloverFunc* g_showRollover{ nullptr };
	ShowButtonFunc* g_showActivateButton{ nullptr };
	ShowButtonFunc* g_showSecondaryButton{ nullptr };
	ShowButtonFunc* g_showRolloverNoun{ nullptr };
	RuntimeHookWatch::Stats g_rolloverWatch;
	RuntimeHookWatch::Stats g_activateWatch;
	RuntimeHookWatch::Stats g_secondaryWatch;
	RuntimeHookWatch::Stats g_nounWatch;
	std::atomic_bool g_installed{ false };
	std::atomic_bool g_traceEnabled{ false };
	std::atomic_uint32_t g_traceLines{ 0 };
	std::mutex g_seenLock;
	std::unordered_set<std::string> g_seen;

	[[nodiscard]] std::string escape(std::string_view text)
	{
		std::string out;
		out.reserve(text.size());
		for (const auto ch : text)
		{
			switch (ch)
			{
			case '\\': out += "\\\\"; break;
			case '"': out += "\\\""; break;
			case '\r': out += "\\r"; break;
			case '\n': out += "\\n"; break;
			case '\t': out += "\\t"; break;
			default: out.push_back(ch); break;
			}
		}
		return out;
	}

	[[nodiscard]] std::string pointerText(const void* pointer)
	{
		return pointer ? std::format("{:X}", reinterpret_cast<std::uintptr_t>(pointer)) : "0";
	}

	[[nodiscard]] std::optional<std::string_view> readableCString(const char* text, std::size_t maxBytes)
	{
		if (!text || maxBytes == 0)
		{
			return std::nullopt;
		}

		MEMORY_BASIC_INFORMATION info{};
		if (::VirtualQuery(text, std::addressof(info), sizeof(info)) == 0)
		{
			return std::nullopt;
		}

		const auto begin = reinterpret_cast<std::uintptr_t>(text);
		const auto regionBegin = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
		const auto regionEnd = regionBegin + info.RegionSize;
		if (begin < regionBegin || begin >= regionEnd)
		{
			return std::nullopt;
		}
		if (info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
		{
			return std::nullopt;
		}

		const auto available = std::min<std::size_t>(maxBytes, regionEnd - begin);
		const auto* nul = static_cast<const char*>(std::memchr(text, '\0', available));
		if (!nul || nul == text)
		{
			return std::nullopt;
		}
		return std::string_view{ text, static_cast<std::size_t>(nul - text) };
	}

	[[nodiscard]] std::optional<std::wstring_view> readableWideCString(const wchar_t* text, std::size_t maxChars)
	{
		if (!text || maxChars == 0 || reinterpret_cast<std::uintptr_t>(text) % alignof(wchar_t) != 0)
		{
			return std::nullopt;
		}

		MEMORY_BASIC_INFORMATION info{};
		if (::VirtualQuery(text, std::addressof(info), sizeof(info)) == 0)
		{
			return std::nullopt;
		}

		const auto begin = reinterpret_cast<std::uintptr_t>(text);
		const auto regionBegin = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
		const auto regionEnd = regionBegin + info.RegionSize;
		if (begin < regionBegin || begin >= regionEnd)
		{
			return std::nullopt;
		}
		if (info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
		{
			return std::nullopt;
		}

		const auto availableBytes = std::min<std::size_t>(maxChars * sizeof(wchar_t), regionEnd - begin);
		const auto availableChars = availableBytes / sizeof(wchar_t);
		for (std::size_t length = 0; length < availableChars; ++length)
		{
			if (text[length] == L'\0')
			{
				return length == 0 ? std::nullopt : std::optional{ std::wstring_view{ text, length } };
			}
			const auto ch = static_cast<std::uint32_t>(text[length]);
			if (ch < 0x20 && text[length] != L'\t')
			{
				return std::nullopt;
			}
		}
		return std::nullopt;
	}

	[[nodiscard]] const RE::BSStringPool::Entry* readableStringPoolLeaf(const void* entryPointer, std::size_t maxChars)
	{
		auto* entry = static_cast<const RE::BSStringPool::Entry*>(entryPointer);
		for (std::uint32_t depth = 0; depth < 8; ++depth)
		{
			if (!RuntimeMemorySafety::IsReadableMemory(entry, sizeof(RE::BSStringPool::Entry)))
			{
				return nullptr;
			}
			if (entry->shallow())
			{
				entry = entry->_right;
				continue;
			}
			const auto length = entry->_length;
			return length != 0 && length <= maxChars ? entry : nullptr;
		}
		return nullptr;
	}

	[[nodiscard]] std::optional<std::string_view> readableFixedStringChar(const void* entryPointer)
	{
		const auto* entry = readableStringPoolLeaf(entryPointer, kMaxTextBytes);
		if (!entry || entry->wide())
		{
			return std::nullopt;
		}
		const auto length = entry->_length;
		const auto* text = reinterpret_cast<const char*>(entry + 1);
		if (!RuntimeMemorySafety::IsReadableMemory(text, length + 1) || text[length] != '\0')
		{
			return std::nullopt;
		}
		return std::string_view{ text, length };
	}

	[[nodiscard]] std::optional<std::wstring_view> readableFixedStringWide(const void* entryPointer)
	{
		const auto* entry = readableStringPoolLeaf(entryPointer, kMaxWideChars);
		if (!entry || !entry->wide())
		{
			return std::nullopt;
		}
		const auto length = entry->_length;
		const auto* text = reinterpret_cast<const wchar_t*>(entry + 1);
		if (!RuntimeMemorySafety::IsReadableMemory(text, (static_cast<std::size_t>(length) + 1) * sizeof(wchar_t)) ||
			text[length] != L'\0')
		{
			return std::nullopt;
		}
		return std::wstring_view{ text, length };
	}

	[[nodiscard]] bool shouldLogText(std::wstring_view wide) noexcept
	{
		return g_traceEnabled.load(std::memory_order_relaxed) &&
			RuntimeInGameTextDictionary::ShouldCapture(wide);
	}

	[[nodiscard]] bool remember(std::string key)
	{
		std::scoped_lock lock{ g_seenLock };
		return g_seen.emplace(std::move(key)).second;
	}

	[[nodiscard]] const RE::TESForm* liveForm(const void* pointer)
	{
		const auto* form = static_cast<const RE::TESForm*>(pointer);
		if (!RuntimeMemorySafety::IsReadableMemory(form, sizeof(RE::TESForm)) || form->formID == 0)
		{
			return nullptr;
		}
		const auto* live = RE::TESForm::GetFormByID(form->formID);
		return live == form ? live : nullptr;
	}

	[[nodiscard]] std::string formText(const RE::TESForm* form)
	{
		if (!form)
		{
			return {};
		}
		const auto* editor = form->GetFormEditorID();
		return std::format(
			"{:08X}/{}{}{}",
			static_cast<std::uint32_t>(form->formID),
			form->GetFormTypeString(),
			editor && editor[0] ? "/" : "",
			editor ? editor : "");
	}

	[[nodiscard]] std::string scanForms(const void* object, std::string_view label)
	{
		if (!RuntimeMemorySafety::IsReadableMemory(object, kFormScanBytes))
		{
			return {};
		}

		std::string out;
		for (std::size_t offset = 0; offset + sizeof(void*) <= kFormScanBytes; offset += sizeof(void*))
		{
			const auto value = RuntimeMemorySafety::ReadUnaligned<std::uintptr_t>(object, offset);
			if (!value || *value < 0x10000)
			{
				continue;
			}
			const auto* form = liveForm(reinterpret_cast<const void*>(*value));
			if (!form)
			{
				continue;
			}
			if (!out.empty())
			{
				out += "; ";
			}
			out += std::format("{}+{:X}={}", label, offset, formText(form));
			if (out.size() > 360)
			{
				break;
			}
		}
		return out;
	}

	void writeTextTrace(
		std::string_view hook,
		std::string_view stage,
		const void* rollover,
		const void* parameters,
		std::string_view slot,
		std::string_view kind,
		std::wstring_view wide,
		const void* caller)
	{
		if (!shouldLogText(wide) || g_traceLines.fetch_add(1, std::memory_order_relaxed) >= kTraceLineLimit)
		{
			return;
		}

		const auto text = RuntimeInGameTextDictionary::WideToUtf8(wide);
		const auto activation = RuntimeActivationLookContext::FormatRecent(1000);
		const auto forms = scanForms(parameters, "params");
		auto allForms = scanForms(rollover, "rollover");
		if (!forms.empty())
		{
			if (!allForms.empty())
			{
				allForms += "; ";
			}
			allForms += forms;
		}

		const auto key = std::format("{}|{}|{}|{}|{}|{}", hook, stage, slot, kind, text, allForms);
		if (!remember(key))
		{
			return;
		}

		REX::INFO(
			"{} hud-rollover trace hook={} stage={} rollover={} params={} slot={} kind={} text=\"{}\" textLen={} caller=\"{}\" forms=\"{}\" activation=\"{}\"",
			Plugin::NAME,
			hook,
			stage,
			pointerText(rollover),
			pointerText(parameters),
			slot,
			kind,
			escape(text),
			wide.size(),
			escape(RuntimeInGameTextLog::FormatAddress(caller)),
			escape(allForms),
			escape(activation));
	}

	void tracePointerSlot(std::string_view hook, void* rollover, void* parameters, std::size_t offset, const void* caller)
	{
		const auto value = RuntimeMemorySafety::ReadUnaligned<std::uintptr_t>(parameters, offset);
		if (!value || *value < 0x10000)
		{
			return;
		}

		const auto* pointer = reinterpret_cast<const void*>(*value);
		const auto slot = std::format("+{:X}", offset);
		if (auto fixed = readableFixedStringChar(pointer))
		{
			const auto wide = RuntimeInGameTextDictionary::Utf8ToWide(*fixed);
			writeTextTrace(hook, "param-slot", rollover, parameters, slot, "BSFixedStringCS", wide, caller);
			return;
		}
		if (auto fixedWide = readableFixedStringWide(pointer))
		{
			writeTextTrace(hook, "param-slot", rollover, parameters, slot, "BSFixedStringWCS", *fixedWide, caller);
			return;
		}
		if (auto cstring = readableCString(static_cast<const char*>(pointer), kMaxTextBytes))
		{
			const auto wide = RuntimeInGameTextDictionary::Utf8ToWide(*cstring);
			writeTextTrace(hook, "param-slot", rollover, parameters, slot, "CString", wide, caller);
			return;
		}
		if (auto wideString = readableWideCString(static_cast<const wchar_t*>(pointer), kMaxWideChars))
		{
			writeTextTrace(hook, "param-slot", rollover, parameters, slot, "WideCString", *wideString, caller);
		}
	}

	void traceRolloverParameters(std::string_view hook, void* rollover, void* parameters, const void* caller)
	{
		if (!g_traceEnabled.load(std::memory_order_relaxed) || !RuntimeMemorySafety::IsReadableMemory(parameters, 0x20))
		{
			return;
		}

		for (const auto& slot : kRolloverTextSlots)
		{
			tracePointerSlot(hook, rollover, parameters, slot.offset, caller);
		}
	}

	void traceButtonText(std::string_view hook, void* rollover, char* text, const void* caller)
	{
		if (!g_traceEnabled.load(std::memory_order_relaxed))
		{
			return;
		}
		const auto source = readableCString(text, kMaxTextBytes);
		if (!source)
		{
			return;
		}
		const auto wide = RuntimeInGameTextDictionary::Utf8ToWide(*source);
		writeTextTrace(hook, "button-arg", rollover, nullptr, "arg1", "CString", wide, caller);
	}

	void showRolloverThunk(void* rollover, void* parameters)
	{
		RuntimeHookWatch::ScopedCall watch{ g_rolloverWatch, "HUDRollover::ShowRollover" };
		const auto context = RuntimeActivationLookContext::LatestRecent(1000);
		const auto patches = context ?
			RuntimeHudRolloverActionPatch::TryApply(parameters, *context) :
			std::vector<RuntimeHudRolloverActionPatch::Patch>{};
		traceRolloverParameters("HUDRollover::ShowRollover", rollover, parameters, _ReturnAddress());
		g_showRollover(rollover, parameters);
		if (!patches.empty())
		{
			RuntimeHudRolloverActionPatch::Restore(patches);
		}
	}

	void showActivateButtonThunk(void* rollover, char* text)
	{
		RuntimeHookWatch::ScopedCall watch{ g_activateWatch, "HUDRollover::ShowActivateButton" };
		traceButtonText("HUDRollover::ShowActivateButton", rollover, text, _ReturnAddress());
		g_showActivateButton(rollover, text);
	}

	void showSecondaryButtonThunk(void* rollover, char* text)
	{
		RuntimeHookWatch::ScopedCall watch{ g_secondaryWatch, "HUDRollover::ShowSecondaryButton" };
		traceButtonText("HUDRollover::ShowSecondaryButton", rollover, text, _ReturnAddress());
		g_showSecondaryButton(rollover, text);
	}

	void showRolloverNounThunk(void* rollover, char* text)
	{
		RuntimeHookWatch::ScopedCall watch{ g_nounWatch, "HUDRollover::ShowRolloverNoun" };
		traceButtonText("HUDRollover::ShowRolloverNoun", rollover, text, _ReturnAddress());
		g_showRolloverNoun(rollover, text);
	}

	template <class Func>
	void installOffset(REL::Offset offset, Func* detour, Func*& original, std::string_view name)
	{
		REL::Relocation<std::uintptr_t> target{ offset };
		const auto result = RuntimePrologueHook::InstallJump(target.address(), reinterpret_cast<std::uintptr_t>(detour), kMaxPatchBytes);
		if (!result.installed)
		{
			REX::ERROR("{} skipped {} hook; unsupported prologue bytes: {}", Plugin::NAME, name, result.prologueBytes);
			return;
		}
		original = reinterpret_cast<Func*>(result.original);
		REX::INFO("{} installed {} hook at {:X} offset={:X}.", Plugin::NAME, name, target.address(), offset.offset());
	}
}

namespace RuntimeHudRolloverHook
{
	void Install()
	{
		bool expected = false;
		if (!g_installed.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
		{
			return;
		}
		g_traceLines.store(0, std::memory_order_relaxed);
		const auto settings = RuntimeApplySettings::Load();
		g_traceEnabled.store(settings.TraceEnabled(), std::memory_order_relaxed);
		RuntimeHudRolloverActionPatch::ConfigureTrace(settings.TraceEnabled());
		{
			std::scoped_lock lock{ g_seenLock };
			g_seen.clear();
		}

		REL::Relocation<std::uintptr_t> rolloverTarget{ kShowRolloverID };
		const auto rollover = RuntimePrologueHook::InstallJump(
			rolloverTarget.address(),
			reinterpret_cast<std::uintptr_t>(showRolloverThunk),
			kMaxPatchBytes);
		if (!rollover.installed)
		{
			REX::ERROR("{} skipped HUDRollover::ShowRollover hook; unsupported prologue bytes: {}", Plugin::NAME, rollover.prologueBytes);
		}
		else
		{
			g_showRollover = reinterpret_cast<ShowRolloverFunc*>(rollover.original);
			REX::INFO("{} installed HUDRollover::ShowRollover hook at {:X} using Address Library ID {}.", Plugin::NAME, rolloverTarget.address(), kShowRolloverID.id());
		}

		installOffset(kShowActivateButtonOffset, showActivateButtonThunk, g_showActivateButton, "HUDRollover::ShowActivateButton");
		installOffset(kShowSecondaryButtonOffset, showSecondaryButtonThunk, g_showSecondaryButton, "HUDRollover::ShowSecondaryButton");
		installOffset(kShowRolloverNounOffset, showRolloverNounThunk, g_showRolloverNoun, "HUDRollover::ShowRolloverNoun");
	}
}
