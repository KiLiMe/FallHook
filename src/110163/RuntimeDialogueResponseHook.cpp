// AI CONTEXT: Hooks dialogue response construction for lazy INFO:NAM1 spoken-line mutation.
// Depends on RuntimeDialogueResponseTranslations, RuntimeHookWatch, and RuntimePrologueHook.
// Runtime scope is Fallout 4 1.10.163 only; uses verified Address Library ID 755245.
// Version-specific logic: installs the 1.10.163 DialogueResponse constructor prologue hook.
// Source-free policy: delegates to source-free response maps; no visible/source text matching.
#include "PCH.h"

#include "110163/RuntimeDialogueResponseHook.h"

#include "RuntimeActivityWatch.h"
#include "RuntimeHookWatch.h"
#include "RuntimePrologueHook.h"
#include "110163/RuntimeDialogueResponseTranslations.h"

#include "RE/D/DialogueResponse.h"
#include "RE/T/TESTopicInfo.h"
#include "RE/T/TESObjectREFR.h"
#include "RE/T/TESQuest.h"
#include "RE/T/TESResponse.h"
#include "RE/T/TESTopic.h"

#include <atomic>

namespace
{
	using DialogueResponseCtorFunc = RE::DialogueResponse*(
		RE::DialogueResponse*,
		RE::TESTopic*,
		RE::TESTopicInfo*,
		RE::TESObjectREFR*,
		RE::TESResponse*,
		RE::TESQuest*);

	constexpr REL::ID kDialogueResponseCtorID{ 755245 };
	constexpr std::size_t kMaxPatchBytes{ 16 };

	DialogueResponseCtorFunc* g_dialogueResponseCtor{ nullptr };
	RuntimeHookWatch::Stats g_ctorWatch;
	std::atomic_bool g_installed{ false };

	RE::DialogueResponse* dialogueResponseCtorThunk(
		RE::DialogueResponse* out,
		RE::TESTopic* topic,
		RE::TESTopicInfo* topicInfo,
		RE::TESObjectREFR* speaker,
		RE::TESResponse* response,
		RE::TESQuest* quest)
	{
		RuntimeHookWatch::ScopedCall watch{ g_ctorWatch, "DialogueResponse::DialogueResponse" };
		auto* result = g_dialogueResponseCtor(out, topic, topicInfo, speaker, response, quest);
		if (!result)
		{
			result = out;
		}
		RuntimeActivityWatch::RunWork(
			"DialogueResponse apply constructed response",
			[&]() { (void)RuntimeDialogueResponseTranslations::ApplyConstructedResponse(result, topicInfo, response); });
		return result;
	}

	template <class Func>
	void installID(REL::ID id, Func* detour, Func*& original, std::string_view name)
	{
		REL::Relocation<std::uintptr_t> target{ id };
		const auto result = RuntimePrologueHook::InstallJump(
			target.address(),
			reinterpret_cast<std::uintptr_t>(detour),
			kMaxPatchBytes);
		if (!result.installed)
		{
			REX::ERROR(
				"{} skipped {} hook; unsupported prologue bytes: {}",
				Plugin::NAME,
				name,
				result.prologueBytes);
			return;
		}

		original = reinterpret_cast<Func*>(result.original);
		REX::INFO("{} installed {} hook at {:X} using Address Library ID {}.", Plugin::NAME, name, target.address(), id.id());
	}
}

namespace RuntimeDialogueResponseHook
{
	void Install()
	{
		bool expected = false;
		if (!g_installed.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
		{
			return;
		}

		installID(
			kDialogueResponseCtorID,
			dialogueResponseCtorThunk,
			g_dialogueResponseCtor,
			"DialogueResponse::DialogueResponse");
	}
}
