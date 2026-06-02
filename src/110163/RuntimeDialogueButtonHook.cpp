// AI CONTEXT: Hooks player dialogue context and DialogueMenuUtils button text for 1.10.163 choices.
// Depends on RuntimeDialogueChoiceTranslations, RuntimeHookWatch, and RuntimePrologueHook.
// Runtime scope is Fallout 4 1.10.163 player dialogue menu buttons only.
// Version-specific logic: installs 1.10.163 hooks at Address Library IDs 781358 and 643840.
// Source-free policy: uses TESTopicInfo/TESTopic form identity context; incoming visible text is diagnostic only.
#include "PCH.h"

#include "110163/RuntimeDialogueButtonHook.h"

#include "RuntimeActivityWatch.h"
#include "RuntimeApplySettings.h"
#include "RuntimeHookWatch.h"
#include "RuntimePrologueHook.h"
#include "110163/RuntimeDialogueChoiceContext.h"
#include "110163/RuntimeDialogueChoiceTranslations.h"

#include "RE/B/BGSSceneActionPlayerDialogue.h"
#include "RE/B/BSStringT.h"
#include "RE/T/TESObjectREFR.h"
#include "RE/T/TESTopicInfo.h"

#include <atomic>
#include <limits>

namespace
{
	using GetCurrentTopicInfoFunc = RE::TESTopicInfo*(
		RE::BGSSceneActionPlayerDialogue*,
		RE::BGSScene*,
		RE::TESObjectREFR*,
		std::uint32_t);
	using SetButtonTextFunc = void(std::uint8_t, RE::BSString&, std::uint8_t, bool);

	constexpr REL::ID kGetCurrentTopicInfoID{ 781358 };
	constexpr REL::ID kSetButtonTextID{ 643840 };
	constexpr std::size_t kMaxPatchBytes{ 16 };
	constexpr std::size_t kButtonCount{ 4 };
	constexpr std::uint32_t kTraceLimit{ 192 };

	using ChoiceContext = RuntimeDialogueChoiceTranslations::ChoiceContext;

	GetCurrentTopicInfoFunc* g_getCurrentTopicInfo{ nullptr };
	SetButtonTextFunc* g_setButtonText{ nullptr };
	RuntimeHookWatch::Stats g_contextWatch;
	RuntimeHookWatch::Stats g_buttonWatch;
	RuntimeHookWatch::Stats g_buttonWorkWatch;
	std::atomic_bool g_installed{ false };
	std::atomic_bool g_traceEnabled{ false };
	std::atomic_uint32_t g_traceLines{ 0 };

	[[nodiscard]] bool isPlayerResponseType(std::uint32_t responseType) noexcept
	{
		return responseType < kButtonCount;
	}

	void traceContext(std::string_view stage, std::uint8_t button, const ChoiceContext& context, std::size_t sourceLen = 0)
	{
		if (!g_traceEnabled.load(std::memory_order_relaxed) ||
			g_traceLines.fetch_add(1, std::memory_order_relaxed) >= kTraceLimit)
		{
			return;
		}
		REX::INFO(
			"{} dialogue-button trace stage={} button={} type={} info0={:08X} info1={:08X} topic={:08X} quest={:08X} sourceLen={}",
			Plugin::NAME,
			stage,
			button,
			context.responseType,
			context.infoCount > 0 ? context.infoFormIDs[0] : 0,
			context.infoCount > 1 ? context.infoFormIDs[1] : 0,
			context.topicFormID,
			context.questFormID,
			sourceLen);
	}

	void traceResult(std::string_view stage, std::uint8_t button, const RuntimeDialogueChoiceTranslations::LookupResult& result)
	{
		if (!g_traceEnabled.load(std::memory_order_relaxed) ||
			g_traceLines.fetch_add(1, std::memory_order_relaxed) >= kTraceLimit)
		{
			return;
		}
		REX::INFO(
			"{} dialogue-button trace stage={} button={} mode={} form={:08X} textLen={}",
			Plugin::NAME,
			stage,
			button,
			result.mode,
			result.formID,
			result.text.size());
	}

	void rememberContext(std::uint32_t responseType, RE::TESTopicInfo* info)
	{
		if (!isPlayerResponseType(responseType) || !info)
		{
			return;
		}
		const auto captured = RuntimeDialogueChoiceContext::Make(info, responseType);
		const auto& context = captured.choice;
		if (context.infoCount == 0 && context.topicFormID == 0)
		{
			return;
		}

		RuntimeDialogueChoiceContext::Remember(responseType, captured);
		traceContext("context", static_cast<std::uint8_t>(responseType), context);
	}

	RE::TESTopicInfo* getCurrentTopicInfoThunk(
		RE::BGSSceneActionPlayerDialogue* action,
		RE::BGSScene* parentScene,
		RE::TESObjectREFR* target,
		std::uint32_t responseType)
	{
		RuntimeHookWatch::ScopedCall watch{ g_contextWatch, "BGSSceneActionPlayerDialogue::GetCurrentTopicInfo" };
		auto* result = g_getCurrentTopicInfo(action, parentScene, target, responseType);
		rememberContext(responseType, result);
		return result;
	}

	void setButtonTextThunk(std::uint8_t button, RE::BSString& text, std::uint8_t challengeLevel, bool isEnabled)
	{
		RuntimeHookWatch::ScopedCall watch{ g_buttonWatch, "DialogueMenuUtils::SetButtonText" };
		if (button >= kButtonCount || text.empty())
		{
			g_setButtonText(button, text, challengeLevel, isEnabled);
			return;
		}

		RuntimeHookWatch::ScopedCall workWatch{ g_buttonWorkWatch, "DialogueMenuUtils::SetButtonText FallHook work" };
		const auto sourceLen = text.length();
		const auto contexts = RuntimeDialogueChoiceContext::Snapshot(button);
		for (const auto& captured : contexts)
		{
			const auto& context = captured.choice;
			if (context.infoCount == 0 && context.topicFormID == 0)
			{
				continue;
			}
			traceContext("button", button, context, sourceLen);
			auto result = RuntimeActivityWatch::RunWork(
				"DialogueMenuUtils choice lookup",
				[&]() { return RuntimeDialogueChoiceTranslations::Resolve(context); });
			if (!result.translated || result.text.empty() || result.text.size() > std::numeric_limits<std::uint16_t>::max() - 1)
			{
				continue;
			}
			RuntimeActivityWatch::RunWork(
				"DialogueMenuUtils choice assign",
				[&]() { text.Set(result.text.c_str(), result.text.size() + 1); });
			traceResult("hit", button, result);
			break;
		}
		g_setButtonText(button, text, challengeLevel, isEnabled);
	}

	[[nodiscard]] std::uintptr_t installHook(REL::ID id, std::uintptr_t detour, const char* name)
	{
		REL::Relocation<std::uintptr_t> target{ id };
		const auto result = RuntimePrologueHook::InstallJump(target.address(), detour, kMaxPatchBytes);
		if (!result.installed)
		{
			REX::ERROR("{} skipped {} hook; unsupported prologue bytes: {}", Plugin::NAME, name, result.prologueBytes);
			return 0;
		}
		REX::INFO("{} installed {} hook at {:X} using Address Library ID {}.", Plugin::NAME, name, target.address(), id.id());
		return result.original;
	}
}

namespace RuntimeDialogueButtonHook
{
	void Install()
	{
		bool expected = false;
		if (!g_installed.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
		{
			return;
		}

		g_traceEnabled.store(RuntimeApplySettings::Load().TraceEnabled(), std::memory_order_relaxed);
		g_getCurrentTopicInfo = reinterpret_cast<GetCurrentTopicInfoFunc*>(installHook(
			kGetCurrentTopicInfoID,
			reinterpret_cast<std::uintptr_t>(getCurrentTopicInfoThunk),
			"BGSSceneActionPlayerDialogue::GetCurrentTopicInfo"));
		g_setButtonText = reinterpret_cast<SetButtonTextFunc*>(installHook(
			kSetButtonTextID,
			reinterpret_cast<std::uintptr_t>(setButtonTextThunk),
			"DialogueMenuUtils::SetButtonText"));
	}
}
