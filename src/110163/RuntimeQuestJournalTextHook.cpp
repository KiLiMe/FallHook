// AI CONTEXT: Applies QUST:CNAM translations at TESQuest::GetJournalTextForStageItem.
// Depends on RuntimeCatalogProvider, RuntimeQuestLogTranslations, and prologue hook support.
// Runtime scope is Fallout 4 1.10.163 Address Library ID 139753 only.
// Version-specific logic: fixed 1.10.163 quest journal stage/item text function.
// Source-free policy: lookup uses quest FormID plus stage/item-derived index only; Source text is not read.
#include "PCH.h"

#include "RuntimeApplySettings.h"
#include "RuntimeActivityWatch.h"
#include "110163/RuntimeCatalogProvider.h"
#include "RuntimeHookWatch.h"
#include "RuntimeMemorySafety.h"
#include "RuntimePrologueHook.h"
#include "110163/RuntimeQuestJournalTextHook.h"
#include "110163/RuntimeQuestLogTranslations.h"

#include "RE/T/TESQuest.h"

#include <Windows.h>
#include <array>
#include <atomic>
#include <mutex>

#ifdef ERROR
#	undef ERROR
#endif

namespace
{
	using GetJournalTextForStageItemFunc = void(RE::TESQuest*, RE::BSString&, std::uint16_t, std::uint8_t);

	constexpr REL::ID kGetJournalTextForStageItemID{ 139753 };
	constexpr std::size_t kMaxPatchBytes{ 16 };
	constexpr std::uint32_t kTraceLimit{ 256 };

	GetJournalTextForStageItemFunc* g_originalGetJournalTextForStageItem{ nullptr };
	RuntimeHookWatch::Stats g_hookWatch;
	std::mutex g_catalogLock;
	std::atomic_bool g_catalogReady{ false };
	std::atomic_bool g_traceEnabled{ false };
	std::atomic_uint32_t g_traceLines{ 0 };
	std::atomic_uint32_t g_traceSuppressed{ 0 };
	bool g_installed{ false };

	void ensureTranslations()
	{
		if (g_catalogReady.load(std::memory_order_acquire) || !RuntimeCatalogProvider::HasLoadedPluginList())
		{
			return;
		}

		std::scoped_lock lock{ g_catalogLock };
		if (g_catalogReady.load(std::memory_order_relaxed))
		{
			return;
		}

		const auto* catalogResult = RuntimeCatalogProvider::Ensure("RuntimeQuestJournalTextHook build translation map");
		if (!catalogResult)
		{
			REX::WARN("{} quest-journal-stage hook could not build catalog yet.", Plugin::NAME);
			return;
		}

		RuntimeQuestLogTranslations::Rebuild(catalogResult->prepared.questJournal);
		g_catalogReady.store(true, std::memory_order_release);
	}

	[[nodiscard]] std::uint32_t formID(const RE::TESQuest* quest) noexcept
	{
		return quest ? static_cast<std::uint32_t>(quest->formID) : 0;
	}

	[[nodiscard]] const char* editorID(const RE::TESQuest* quest) noexcept
	{
		if (!RuntimeMemorySafety::IsReadableMemory(quest, sizeof(RE::TESForm)))
		{
			return "";
		}
		const auto* editor = quest->GetFormEditorID();
		return editor ? editor : "";
	}

	[[nodiscard]] std::uintptr_t executableBase() noexcept
	{
		static const auto base = reinterpret_cast<std::uintptr_t>(::GetModuleHandleW(nullptr));
		return base;
	}

	[[nodiscard]] std::uintptr_t callerRVA(const void* caller) noexcept
	{
		const auto address = reinterpret_cast<std::uintptr_t>(caller);
		const auto base = executableBase();
		return address >= base ? address - base : 0;
	}

	[[nodiscard]] const char* lookupTranslation(
		const RE::TESQuest* quest,
		std::uint16_t stage,
		std::uint8_t item,
		std::uint32_t& lookupIndex)
	{
		lookupIndex = 0xFFFFFFFFu;
		const auto questID = formID(quest);
		if (questID == 0)
		{
			return nullptr;
		}
		const auto lookup = RuntimeActivityWatch::RunWork(
			"QuestJournal direct lookup",
			[&]() { return RuntimeQuestLogTranslations::LookupByStageItem(questID, stage, item); });
		lookupIndex = lookup.index;
		return lookup.text;
	}

	void trace(
		std::string_view stage,
		const RE::TESQuest* quest,
		std::uint16_t stageIndex,
		std::uint8_t itemIndex,
		std::uint32_t lookupIndex,
		std::size_t originalLen,
		std::size_t translatedLen,
		const void* caller)
	{
		if (!g_traceEnabled.load(std::memory_order_relaxed))
		{
			return;
		}

		const auto previous = g_traceLines.fetch_add(1, std::memory_order_relaxed);
		if (previous >= kTraceLimit)
		{
			const auto suppressed = g_traceSuppressed.fetch_add(1, std::memory_order_relaxed) + 1;
			if (previous == kTraceLimit)
			{
				REX::INFO("{} quest-journal-stage trace stage=limit-reached cap={} suppressed={}", Plugin::NAME, kTraceLimit, suppressed);
			}
			return;
		}

		REX::INFO(
			"{} quest-journal-stage trace stage={} caller={:016X} callerRva={:08X} quest={:08X} editor={} stageIndex={} itemIndex={} lookupIndex={} originalLen={} destLen={}",
			Plugin::NAME,
			stage,
			reinterpret_cast<std::uintptr_t>(caller),
			callerRVA(caller),
			formID(quest),
			editorID(quest),
			stageIndex,
			itemIndex,
			lookupIndex,
			originalLen,
			translatedLen);
	}

	void getJournalTextForStageItemThunk(RE::TESQuest* quest, RE::BSString& out, std::uint16_t stage, std::uint8_t item)
	{
		const auto* caller = _ReturnAddress();
		RuntimeHookWatch::ScopedCall hookWatch{ g_hookWatch, "TESQuest::GetJournalTextForStageItem" };
		g_originalGetJournalTextForStageItem(quest, out, stage, item);
		RuntimeActivityWatch::RunWork("QuestJournal ensure translations", []() { ensureTranslations(); });

		std::uint32_t lookupIndex = 0xFFFFFFFFu;
		const auto* translation = RuntimeActivityWatch::RunWork(
			"QuestJournal lookup",
			[&]() { return lookupTranslation(quest, stage, item, lookupIndex); });
		const auto originalLen = out.length();
		if (!translation || *translation == '\0')
		{
			trace("miss", quest, stage, item, lookupIndex, originalLen, 0, caller);
			return;
		}

		const auto translatedLen = std::char_traits<char>::length(translation);
		RuntimeActivityWatch::RunWork("QuestJournal assign", [&]() { out.Set(translation, translatedLen + 1); });
		trace("hit-assign", quest, stage, item, lookupIndex, originalLen, translatedLen, caller);
	}
}

namespace RuntimeQuestJournalTextHook
{
	void Install()
	{
		if (g_installed)
		{
			return;
		}

		g_traceEnabled.store(RuntimeApplySettings::Load().TraceEnabled(), std::memory_order_relaxed);
		g_catalogReady.store(false, std::memory_order_relaxed);
		g_traceLines.store(0, std::memory_order_relaxed);
		g_traceSuppressed.store(0, std::memory_order_relaxed);

		REL::Relocation<std::uintptr_t> target{ kGetJournalTextForStageItemID };
		const auto result = RuntimePrologueHook::InstallJump(
			target.address(),
			reinterpret_cast<std::uintptr_t>(getJournalTextForStageItemThunk),
			kMaxPatchBytes);
		if (!result.installed)
		{
			REX::ERROR("{} skipped TESQuest::GetJournalTextForStageItem hook; unsupported prologue bytes: {}", Plugin::NAME, result.prologueBytes);
			return;
		}

		g_originalGetJournalTextForStageItem = reinterpret_cast<GetJournalTextForStageItemFunc*>(result.original);
		g_installed = true;
		REX::INFO(
			"{} installed TESQuest::GetJournalTextForStageItem translation hook at {:X} using Address Library ID {}.",
			Plugin::NAME,
			target.address(),
			kGetJournalTextForStageItemID.id());
	}
}
