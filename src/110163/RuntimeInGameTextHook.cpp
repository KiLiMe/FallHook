// AI CONTEXT: Installs the approved source-keyed TXT hook for global in-game UI text.
// Depends on RuntimeInGameTextDictionary/Log/ScaleformStream and 1.10.163 prologue hooks.
// Runtime scope is Fallout 4 1.10.163 AddTranslations/AddTranslation and literal-only BSScaleformTranslator::Translate.
// Version-specific logic: fixed 1.10.163 Address Library IDs only, including Translate TerminalMenu bypass IDs.
// Source-free policy: this is isolated to [InGameTextHook] TXT source keys; XML lookup remains source-free.
#include "PCH.h"

#include "110163/RuntimeInGameTextHook.h"

#include "RuntimeActivityWatch.h"
#include "RuntimeHookWatch.h"
#include "RuntimeInGameTextDictionary.h"
#include "RuntimeInGameTextLog.h"
#include "RuntimeMemorySafety.h"
#include "110163/RuntimeInGameTextHookPatch.h"
#include "110163/RuntimeInGameTextSafeCopy.h"
#include "110163/RuntimeInGameTextScaleformStream.h"
#include "110163/RuntimeInGameTextTranslateCache.h"

#include "RE/B/BSFixedString.h"
#include "RE/B/BSScaleformTranslator.h"
#include "RE/B/BSStreamParser.h"
#include "RE/T/TerminalMenu.h"
#include "RE/U/UI.h"

#include <Windows.h>

#ifdef ERROR
#	undef ERROR
#endif

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace
{
	struct ScaleformTranslateInfo
	{
		const wchar_t* key{ nullptr };
		void* result{ nullptr };
		const char* instanceName{ nullptr };
		std::uint8_t flags{ 0 };
		std::uint8_t pad19{ 0 };
		std::uint16_t pad1A{ 0 };
		std::uint32_t pad1C{ 0 };
	};
	static_assert(sizeof(ScaleformTranslateInfo) == 0x20);

	using AddTranslationsFunc = void(RE::BSScaleformTranslator*, RE::BSStreamParser<wchar_t>*);
	using AddTranslationFunc = void(RE::BSTranslator*, RE::BSFixedStringWCS&, RE::BSFixedStringWCS&);
	using SetResultFunc = void(ScaleformTranslateInfo*, const wchar_t*, std::uint64_t);
	using TranslateFunc = void(RE::BSScaleformTranslator*, ScaleformTranslateInfo*);
	using UIGetMenuOpenFunc = bool(RE::UI*, const RE::BSFixedString&);

	constexpr REL::ID kAddTranslationsID{ 810671 };
	constexpr REL::ID kAddTranslationID{ 266342 };
	constexpr REL::ID kSetResultID{ 327619 };
	constexpr REL::ID kUISingletonID{ 548587 };
	constexpr REL::ID kUIGetMenuOpenID{ 1065114 };
	constexpr std::size_t kTranslateVtableSlot{ 2 };

	std::atomic_bool g_active{ false };
	std::atomic_bool g_translateEnabled{ false };
	std::atomic_bool g_installed{ false };
	std::mutex g_applyLock;
	std::mutex g_aliasLock;
	std::unordered_map<RE::BSScaleformTranslator*, std::uint64_t> g_appliedRevision;
	std::unordered_map<std::wstring, std::wstring> g_literalToKey;
	std::unordered_map<std::wstring, std::wstring> g_keyToLiteral;
	thread_local bool g_dictionaryApplyActive = false;
	thread_local std::wstring g_setResultReplacement;

	AddTranslationsFunc* g_addTranslations{ nullptr };
	AddTranslationFunc* g_addTranslation{ nullptr };
	SetResultFunc* g_setResult{ nullptr };
	TranslateFunc* g_translate{ nullptr };

	RuntimeHookWatch::Stats g_addTranslationsWatch;
	RuntimeHookWatch::Stats g_addTranslationWatch;
	RuntimeHookWatch::Stats g_translateWatch;

	[[nodiscard]] bool shouldProcess() noexcept
	{
		if (!g_active.load(std::memory_order_acquire))
		{
			return false;
		}
		return RuntimeInGameTextLog::Enabled() ||
			(g_translateEnabled.load(std::memory_order_acquire) && RuntimeInGameTextDictionary::EntryCount() != 0);
	}

	[[nodiscard]] std::optional<std::wstring> lookup(std::wstring_view raw)
	{
		if (!g_translateEnabled.load(std::memory_order_acquire))
		{
			return std::nullopt;
		}
		return RuntimeInGameTextDictionary::Lookup(raw);
	}

	[[nodiscard]] bool isRawKey(std::wstring_view text) noexcept
	{
		text = RuntimeInGameTextDictionary::TrimText(text);
		return !text.empty() && text.front() == L'$';
	}

	[[nodiscard]] bool firstCharIsRawKey(const wchar_t* text) noexcept
	{
		const auto ch = RuntimeMemorySafety::ReadUnaligned<wchar_t>(text, 0);
		return ch && *ch == L'$';
	}

	void rememberKeyAlias(std::wstring_view raw, std::wstring_view visible)
	{
		raw = RuntimeInGameTextDictionary::TrimText(raw);
		visible = RuntimeInGameTextDictionary::TrimText(visible);
		if (!isRawKey(raw) || visible.empty() || visible == raw)
		{
			return;
		}

		std::scoped_lock lock{ g_aliasLock };
		g_literalToKey.insert_or_assign(std::wstring{ visible }, std::wstring{ raw });
		g_keyToLiteral.insert_or_assign(std::wstring{ raw }, std::wstring{ visible });
	}

	[[nodiscard]] std::optional<std::wstring> findKeyAlias(std::wstring_view literal)
	{
		literal = RuntimeInGameTextDictionary::TrimText(literal);
		if (literal.empty() || isRawKey(literal))
		{
			return std::nullopt;
		}

		std::scoped_lock lock{ g_aliasLock };
		if (const auto it = g_literalToKey.find(std::wstring{ literal }); it != g_literalToKey.end())
		{
			return it->second;
		}
		return std::nullopt;
	}

	[[nodiscard]] std::optional<std::wstring> findVisibleForKey(std::wstring_view raw)
	{
		raw = RuntimeInGameTextDictionary::TrimText(raw);
		if (!isRawKey(raw))
		{
			return std::nullopt;
		}

		std::scoped_lock lock{ g_aliasLock };
		if (const auto it = g_keyToLiteral.find(std::wstring{ raw }); it != g_keyToLiteral.end())
		{
			return it->second;
		}
		return std::nullopt;
	}

	[[nodiscard]] std::optional<std::wstring> lookupWithAlias(std::wstring_view raw)
	{
		if (auto translated = lookup(raw); translated && !translated->empty())
		{
			return translated;
		}
		if (auto alias = findKeyAlias(raw))
		{
			return lookup(*alias);
		}
		return std::nullopt;
	}

	struct LogText
	{
		std::wstring copy;
		std::wstring text;
	};

	[[nodiscard]] LogText makeLogText(std::wstring_view raw, std::wstring_view visible)
	{
		raw = RuntimeInGameTextDictionary::TrimText(raw);
		visible = RuntimeInGameTextDictionary::TrimText(visible);
		if (isRawKey(raw))
		{
			if (!visible.empty() && visible != raw)
			{
				return { std::wstring{ raw }, std::wstring{ visible } };
			}
			if (auto knownVisible = findVisibleForKey(raw); knownVisible && !knownVisible->empty())
			{
				return { std::wstring{ raw }, std::move(*knownVisible) };
			}
			return { std::wstring{ raw }, std::wstring{ raw } };
		}
		if (isRawKey(visible))
		{
			return { std::wstring{ visible }, std::wstring{ raw.empty() ? visible : raw } };
		}
		if (auto alias = findKeyAlias(raw))
		{
			return { std::move(*alias), std::wstring{ raw } };
		}
		if (auto alias = findKeyAlias(visible))
		{
			return { std::move(*alias), std::wstring{ visible } };
		}
		return { std::wstring{ raw }, std::wstring{ visible.empty() ? raw : visible } };
	}

	void setTranslateResult(ScaleformTranslateInfo* info, std::wstring_view translation)
	{
		if (!g_setResult || !info || translation.empty())
		{
			return;
		}

		g_setResultReplacement.assign(translation);
		g_setResult(info, g_setResultReplacement.data(), static_cast<std::uint64_t>(g_setResultReplacement.size()));
	}

	[[nodiscard]] std::wstring translatorMappedText(RE::BSScaleformTranslator* translator, std::wstring_view raw)
	{
		raw = RuntimeInGameTextDictionary::TrimText(raw);
		if (!translator || raw.empty() || !isRawKey(raw) || !RuntimeMemorySafety::IsReadableMemory(translator, sizeof(RE::BSScaleformTranslator)))
		{
			return {};
		}
		std::unique_lock lock{ g_applyLock, std::try_to_lock };
		if (!lock.owns_lock())
		{
			return {};
		}
		const std::wstring key{ raw };
		RE::BSFixedStringWCS fixedKey{ key.c_str() };
		const auto found = translator->translator.translationMap.find(fixedKey);
		if (found == translator->translator.translationMap.end())
		{
			return {};
		}
		return std::wstring{ RuntimeInGameTextDictionary::TrimText(static_cast<std::wstring_view>(found->second)) };
	}

	[[nodiscard]] bool terminalMenuOpen()
	{
		static std::atomic_uint64_t nextPoll{ 0 };
		static std::atomic_bool cachedOpen{ false };
		const auto now = ::GetTickCount64();
		if (now < nextPoll.load(std::memory_order_acquire))
		{
			return cachedOpen.load(std::memory_order_acquire);
		}

		nextPoll.store(now + 100, std::memory_order_release);
		static REL::Relocation<RE::UI**> uiSingleton{ kUISingletonID };
		static REL::Relocation<UIGetMenuOpenFunc*> getMenuOpen{ kUIGetMenuOpenID };
		static RE::BSFixedString terminalMenuName{ "TerminalMenu" };
		auto* ui = *uiSingleton;
		const bool open = ui && getMenuOpen(ui, terminalMenuName);
		cachedOpen.store(open, std::memory_order_release);
		return open;
	}

	void applyDictionaryToTranslator(RE::BSScaleformTranslator* translator, std::string_view reason)
	{
		if (!translator || !g_addTranslations || !g_translateEnabled.load(std::memory_order_acquire))
		{
			return;
		}
		if (RuntimeInGameTextDictionary::EntryCount() == 0)
		{
			return;
		}

		RuntimeActivityWatch::WorkScope activity{ "InGameText apply TXT dictionary" };
		std::scoped_lock lock{ g_applyLock };
		const auto revision = RuntimeInGameTextDictionary::Revision();
		if (g_appliedRevision[translator] == revision)
		{
			return;
		}

		std::size_t entryCount = 0;
		auto bytes = RuntimeInGameTextDictionary::BuildScaleformUtf16Txt(entryCount);
		if (bytes.empty())
		{
			return;
		}

		RuntimeInGameTextScaleformStream::MemoryStreamParserData data{ std::move(bytes) };
		RE::BSStreamParser<wchar_t> parser{ std::addressof(data) };
		g_dictionaryApplyActive = true;
		g_addTranslations(translator, std::addressof(parser));
		g_dictionaryApplyActive = false;
		g_appliedRevision[translator] = revision;
		REX::INFO(
			"{} applied InGameText TXT dictionary to Scaleform translator reason='{}' entries={} revision={}.",
			Plugin::NAME,
			reason,
			entryCount,
			revision);
	}

	void addTranslationsThunk(RE::BSScaleformTranslator* translator, RE::BSStreamParser<wchar_t>* parser)
	{
		RuntimeHookWatch::ScopedCall watch{ g_addTranslationsWatch, "BSScaleformTranslator::AddTranslations" };
		g_addTranslations(translator, parser);
		applyDictionaryToTranslator(translator, "AddTranslations");
	}

	void addTranslationThunk(RE::BSTranslator* translator, RE::BSFixedStringWCS& key, RE::BSFixedStringWCS& value)
	{
		RuntimeHookWatch::ScopedCall watch{ g_addTranslationWatch, "BSTranslator::AddTranslation" };
		if (!shouldProcess() || g_dictionaryApplyActive)
		{
			g_addTranslation(translator, key, value);
			return;
		}

		const auto raw = RuntimeInGameTextDictionary::TrimText(static_cast<std::wstring_view>(key));
		const auto visible = RuntimeInGameTextDictionary::TrimText(static_cast<std::wstring_view>(value));
		rememberKeyAlias(raw, visible);
		const auto logText = makeLogText(raw, visible);
		if (auto translated = lookupWithAlias(raw); translated && !translated->empty())
		{
			const std::wstring_view display = isRawKey(logText.text) ? std::wstring_view{ *translated } : std::wstring_view{ logText.text };
			RuntimeInGameTextLog::Hit(logText.copy, display, *translated);
			RE::BSFixedStringWCS fixedTranslation{ translated->c_str() };
			g_addTranslation(translator, key, fixedTranslation);
			return;
		}

		if (RuntimeInGameTextDictionary::ShouldCapture(raw) || RuntimeInGameTextDictionary::ShouldCapture(visible))
		{
			RuntimeInGameTextLog::Miss(logText.copy, logText.text);
		}
		g_addTranslation(translator, key, value);
	}

	void translateThunk(RE::BSScaleformTranslator* translator, ScaleformTranslateInfo* info)
	{
		RuntimeHookWatch::ScopedCall watch{ g_translateWatch, "BSScaleformTranslator::Translate" };
		if (!info || !RuntimeMemorySafety::IsReadableMemory(info, sizeof(ScaleformTranslateInfo)) || !shouldProcess())
		{
			g_translate(translator, info);
			return;
		}
		if (!RuntimeInGameTextLog::Enabled() && firstCharIsRawKey(info->key))
		{
			g_translate(translator, info);
			return;
		}
		if (RuntimeActivityWatch::RunWork("InGameText TerminalMenu check", []() { return terminalMenuOpen(); }))
		{
			g_translate(translator, info);
			return;
		}

		const auto rawKey = RuntimeActivityWatch::RunWork(
			"InGameText safe copy",
			[&]() { return RuntimeInGameTextSafeCopy::Wide(info->key); });
		const auto raw = RuntimeInGameTextDictionary::TrimText(rawKey);
		if (isRawKey(raw))
		{
			g_translate(translator, info);
			if (!RuntimeInGameTextLog::Enabled())
			{
				return;
			}

			const auto visible = translatorMappedText(translator, raw);
			const auto hasVisible = !visible.empty() && !isRawKey(visible);
			const auto logText = makeLogText(raw, hasVisible ? std::wstring_view{ visible } : std::wstring_view{});
			if (auto translated = lookup(raw); translated && !translated->empty())
			{
				const std::wstring_view display = isRawKey(logText.text) ? std::wstring_view{ *translated } : std::wstring_view{ logText.text };
				RuntimeInGameTextLog::Hit(logText.copy, display, *translated);
			}
			else if (hasVisible && RuntimeInGameTextDictionary::ShouldCapture(raw))
			{
				RuntimeInGameTextLog::Miss(logText.copy, logText.text);
			}
			return;
		}

		const bool shouldCapture = RuntimeInGameTextDictionary::ShouldCapture(raw);
		auto replacement = shouldCapture && g_translateEnabled.load(std::memory_order_acquire) ?
			RuntimeActivityWatch::RunWork("InGameText TXT lookup", [&]() { return RuntimeInGameTextTranslateCache::LookupLiteral(raw); }) :
			std::optional<std::wstring>{};

		g_translate(translator, info);

		if (replacement && !replacement->empty())
		{
			RuntimeActivityWatch::RunWork("InGameText SetResult", [&]() { setTranslateResult(info, *replacement); });
			const auto logText = makeLogText(raw, raw);
			RuntimeInGameTextLog::Hit(logText.copy, logText.text, *replacement);
			return;
		}

		if (shouldCapture)
		{
			const auto logText = makeLogText(raw, raw);
			RuntimeInGameTextLog::Miss(logText.copy, logText.text);
		}
	}

	template <class Func>
	void installPrologue(REL::ID id, Func* detour, Func*& original, std::string_view name)
	{
		std::uintptr_t originalAddress = 0;
		if (RuntimeInGameTextHookPatch::InstallPrologue(id, reinterpret_cast<std::uintptr_t>(detour), originalAddress, name))
		{
			original = reinterpret_cast<Func*>(originalAddress);
		}
	}

	template <class Func>
	void installAddRcx8JumpStub(REL::ID id, Func* detour, Func*& original)
	{
		std::uintptr_t originalAddress = 0;
		if (RuntimeInGameTextHookPatch::InstallAddRcx8JumpStub(id, reinterpret_cast<std::uintptr_t>(detour), originalAddress))
		{
			original = reinterpret_cast<Func*>(originalAddress);
		}
	}

	void resolveSetResultHelper()
	{
		if (g_setResult)
		{
			return;
		}

		REL::Relocation<std::uintptr_t> target{ kSetResultID };
		g_setResult = reinterpret_cast<SetResultFunc*>(target.address());
		REX::INFO(
			"{} resolved Scaleform::GFx::Translator::TranslateInfo::SetResult helper at {:X} using Address Library ID {}.",
			Plugin::NAME,
			target.address(),
			kSetResultID.id());
	}

	void installTranslateHook()
	{
		REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE::BSScaleformTranslator[0] };
		const auto slotAddress = vtable.address() + sizeof(std::uintptr_t) * kTranslateVtableSlot;
		if (!RuntimeMemorySafety::IsReadableMemory(reinterpret_cast<const void*>(slotAddress), sizeof(std::uintptr_t)))
		{
			REX::ERROR("{} skipped BSScaleformTranslator::Translate hook; unreadable slot {:X}.", Plugin::NAME, slotAddress);
			return;
		}

		g_translate = reinterpret_cast<TranslateFunc*>(vtable.write_vfunc(kTranslateVtableSlot, translateThunk));
		REX::INFO("{} installed BSScaleformTranslator::Translate hook at {:X} slot={}.", Plugin::NAME, vtable.address(), kTranslateVtableSlot);
	}
}

namespace RuntimeInGameTextHook
{
	void Install(const RuntimeInGameTextSettings::Values& settings)
	{
		bool expected = false;
		if (!g_installed.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
		{
			return;
		}

		g_active.store(settings.Active(), std::memory_order_release);
		g_translateEnabled.store(settings.enable, std::memory_order_release);
		REX::INFO(
			"{} InGameTextHook config: Enable={} LogRaw={} active={} txtKeys={}.",
			Plugin::NAME,
			settings.enable,
			settings.logRaw,
			settings.Active(),
			RuntimeInGameTextDictionary::EntryCount());
		if (!settings.Active())
		{
			return;
		}

		if (settings.enable || settings.logRaw)
		{
			installPrologue(kAddTranslationsID, addTranslationsThunk, g_addTranslations, "BSScaleformTranslator::AddTranslations");
			installAddRcx8JumpStub(kAddTranslationID, addTranslationThunk, g_addTranslation);
			resolveSetResultHelper();
			installTranslateHook();
		}
	}
}
