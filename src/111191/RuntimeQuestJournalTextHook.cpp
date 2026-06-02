// AI CONTEXT: Applies QUST:CNAM translations at TESQuestStageItem::GetLogEntry.
// Depends on RuntimeCatalogProvider, RuntimeQuestLogTranslations, and prologue hook support.
// Runtime scope is Fallout 4 1.11.191 quest journal stage/item text support.
// Version-specific logic: fixed 1.11.191 TESQuestStageItem::GetLogEntry hook ID.
// Source-free policy: lookup uses quest FormID plus sID or stage/item identity only; Source text is not read.
#include "PCH.h"

#include "RuntimeApplySettings.h"
#include "111191/RuntimeCatalogProvider.h"
#include "RuntimeHookWatch.h"
#include "RuntimeMemorySafety.h"
#include "RuntimePrologueHook.h"
#include "RuntimeSaveLoadGapTrace.h"
#include "111191/RuntimeQuestJournalTextHook.h"
#include "111191/RuntimeQuestLogTranslations.h"

#include "RE/T/TESQuest.h"

#include <Windows.h>
#include <atomic>
#include <cstring>
#include <mutex>
#include <optional>

#ifdef ERROR
#	undef ERROR
#endif

namespace RE
{
	class TESQuestStageItem;
}

namespace Runtime111191
{
namespace
{
	using GetLogEntryFunc = const char*(RE::TESQuestStageItem*, const RE::TESQuest*);

	constexpr REL::ID kGetLogEntryID{ 2208069 };
	constexpr std::size_t kMaxPatchBytes{ 16 };
	constexpr std::uint32_t kTraceLimit{ 256 };
	constexpr std::size_t kLogEntryStringIDOffset{ 0x10 };
	constexpr std::size_t kStageItemOrdinalOffset{ 0x15 };
	constexpr std::size_t kOwningStageOffset{ 0x20 };
	constexpr std::size_t kQuestStageIndexOffset{ 0x18 };

	struct QuestLogIdentity
	{
		std::uint32_t stringID{ 0 };
		std::optional<std::uint32_t> owningStage;
		std::optional<std::uint32_t> itemIndex;
	};

	GetLogEntryFunc* g_originalGetLogEntry{ nullptr };
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
			REX::WARN("{} quest-log-entry hook could not build catalog yet.", Plugin::NAME);
			return;
		}

		RuntimeQuestLogTranslations::Rebuild(catalogResult->prepared.questJournal);
		g_catalogReady.store(true, std::memory_order_release);
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

	[[nodiscard]] std::uint32_t formID(const RE::TESQuest* quest) noexcept
	{
		return RuntimeMemorySafety::IsReadableMemory(quest, sizeof(RE::TESForm)) ? static_cast<std::uint32_t>(quest->formID) : 0;
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

	template <class T>
	[[nodiscard]] std::optional<T> readAt(const void* base, std::size_t offset)
	{
		if (!RuntimeMemorySafety::IsReadableMemory(base, offset + sizeof(T)))
		{
			return std::nullopt;
		}

		T value{};
		std::memcpy(std::addressof(value), static_cast<const std::byte*>(base) + offset, sizeof(value));
		return value;
	}

	[[nodiscard]] QuestLogIdentity readIdentity(const RE::TESQuestStageItem* item)
	{
		QuestLogIdentity identity;
		if (const auto stringID = readAt<std::uint32_t>(item, kLogEntryStringIDOffset))
		{
			identity.stringID = *stringID;
		}
		if (const auto ordinal = readAt<std::int8_t>(item, kStageItemOrdinalOffset))
		{
			identity.itemIndex = *ordinal < 0 ? 0u : static_cast<std::uint32_t>(*ordinal);
		}

		const auto owningStage = readAt<const void*>(item, kOwningStageOffset);
		if (owningStage && RuntimeMemorySafety::IsReadableMemory(*owningStage, kQuestStageIndexOffset + sizeof(std::uint16_t)))
		{
			std::uint16_t stageIndex = 0;
			std::memcpy(std::addressof(stageIndex), static_cast<const std::byte*>(*owningStage) + kQuestStageIndexOffset, sizeof(stageIndex));
			identity.owningStage = static_cast<std::uint32_t>(stageIndex);
		}
		return identity;
	}

	[[nodiscard]] const char* modeName(RuntimeQuestLogTranslations::LookupMode mode) noexcept
	{
		switch (mode)
		{
		case RuntimeQuestLogTranslations::LookupMode::kStringID:
			return "stringID";
		case RuntimeQuestLogTranslations::LookupMode::kIndex:
			return "index";
		default:
			return "none";
		}
	}

	void trace(
		std::string_view stage,
		const RE::TESQuest* quest,
		const QuestLogIdentity& identity,
		std::uint32_t lookupIndex,
		std::size_t originalLen,
		std::size_t translatedLen,
		const char* mode,
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
				REX::INFO("{} quest-log-entry trace stage=limit-reached cap={} suppressed={}", Plugin::NAME, kTraceLimit, suppressed);
			}
			return;
		}

		const auto currentStage = RuntimeMemorySafety::IsReadableMemory(quest, sizeof(RE::TESQuest)) ? quest->currentStage : 0xFFFFFFFFu;
		REX::INFO(
			"{} quest-log-entry trace stage={} mode={} callerRva={:08X} quest={:08X} editor={} currentStage={} owningStage={} itemIndex={} sID={:08X} lookupIndex={} originalLen={} destLen={}",
			Plugin::NAME,
			stage,
			mode,
			callerRVA(caller),
			formID(quest),
			editorID(quest),
			currentStage,
			identity.owningStage.value_or(0xFFFFFFFFu),
			identity.itemIndex.value_or(0xFFFFFFFFu),
			identity.stringID,
			lookupIndex,
			originalLen,
			translatedLen);
	}

	const char* getLogEntryThunk(RE::TESQuestStageItem* item, const RE::TESQuest* quest)
	{
		const auto* caller = _ReturnAddress();
		RuntimeSaveLoadGapTrace::ScopedHook gapTrace{ "TESQuestStageItem::GetLogEntry" };
		RuntimeHookWatch::ScopedCall hookWatch{ g_hookWatch, "TESQuestStageItem::GetLogEntry" };
		const auto* result = g_originalGetLogEntry(item, quest);
		if (!item || !quest || !result || *result == '\0')
		{
			return result;
		}

		ensureTranslations();

		const auto identity = readIdentity(item);
		const auto currentStage = RuntimeMemorySafety::IsReadableMemory(quest, sizeof(RE::TESQuest)) ?
			static_cast<std::uint32_t>(quest->currentStage) :
			0xFFFFFFFFu;
		const auto lookup = RuntimeQuestLogTranslations::LookupByIdentity(
			formID(quest),
			identity.stringID,
			identity.owningStage,
			identity.itemIndex,
			currentStage);
		const auto originalLen = std::char_traits<char>::length(result);
		if (!lookup.text || *lookup.text == '\0')
		{
			trace("miss", quest, identity, lookup.index, originalLen, 0, modeName(lookup.mode), caller);
			return result;
		}

		const auto translatedLen = std::char_traits<char>::length(lookup.text);
		trace("hit", quest, identity, lookup.index, originalLen, translatedLen, modeName(lookup.mode), caller);
		return lookup.text;
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

		REL::Relocation<std::uintptr_t> target{ kGetLogEntryID };
		const auto result = RuntimePrologueHook::InstallJump(
			target.address(),
			reinterpret_cast<std::uintptr_t>(getLogEntryThunk),
			kMaxPatchBytes);
		if (!result.installed)
		{
			REX::ERROR("{} skipped TESQuestStageItem::GetLogEntry hook; unsupported prologue bytes: {}", Plugin::NAME, result.prologueBytes);
			return;
		}

		g_originalGetLogEntry = reinterpret_cast<GetLogEntryFunc*>(result.original);
		g_installed = true;
		REX::INFO(
			"{} installed TESQuestStageItem::GetLogEntry quest journal translation hook at {:X} using Address Library ID {}.",
			Plugin::NAME,
			target.address(),
			kGetLogEntryID.id());
	}
}

} // namespace Runtime111191
