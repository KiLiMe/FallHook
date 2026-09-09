// AI CONTEXT: Hooks SubtitleManager::ShowSubtitle for source-free INFO:NAM1 subtitle fallback.
// Depends on captured response context, RuntimeDialogueSubtitleTranslations, and RuntimePrologueHook.
// Runtime scope is Fallout 4 1.11.191 subtitles carrying TESTopicInfo identity.
// Version-specific logic: installs a 1.11.191 prologue hook at verified Address Library ID 2249542.
// Source-free policy: uses TESTopicInfo identity; incoming visible text is not a lookup key.
#include "PCH.h"

#include "111191/RuntimeDialogueSubtitleHook.h"

#include "RuntimeActivityWatch.h"
#include "RuntimeHookWatch.h"
#include "RuntimePrologueHook.h"
#include "111191/RuntimeDialogueSubtitleContext.h"
#include "111191/RuntimeDialogueSubtitleTranslations.h"

#include "RE/B/BSFixedString.h"
#include "RE/S/SubtitleManager.h"
#include "RE/T/TESObjectREFR.h"
#include "RE/T/TESTopicInfo.h"

#include <atomic>

namespace Runtime111191
{
namespace
{
	using ShowSubtitleFunc = void(RE::SubtitleManager*, RE::TESObjectREFR*, RE::BSFixedStringCS&, RE::TESTopicInfo*, bool);

	constexpr REL::ID kShowSubtitleID{ 2249542 };
	constexpr std::size_t kMaxPatchBytes{ 16 };

	ShowSubtitleFunc* g_showSubtitle{ nullptr };
	RuntimeHookWatch::Stats g_showSubtitleWatch;
	RuntimeHookWatch::Stats g_showSubtitleWorkWatch;
	std::atomic_bool g_installed{ false };

	void showSubtitleThunk(
		RE::SubtitleManager* manager,
		RE::TESObjectREFR* speaker,
		RE::BSFixedStringCS& text,
		RE::TESTopicInfo* topicInfo,
		bool forceDisplay)
	{
		RuntimeHookWatch::ScopedCall watch{ g_showSubtitleWatch, "SubtitleManager::ShowSubtitle" };
		if (topicInfo && !text.empty())
		{
			RuntimeHookWatch::ScopedCall workWatch{ g_showSubtitleWorkWatch, "SubtitleManager::ShowSubtitle FallHook work" };
			RuntimeActivityWatch::RunWork("SubtitleManager subtitle lookup", [&]() {
				const auto captured = RuntimeDialogueSubtitleContext::Resolve(topicInfo, speaker, text);
				if (captured.translated && !captured.text.empty())
				{
					text = captured.text;
					return;
				}
				const auto liveText = std::string_view{ text.c_str(), text.length() };
				const auto result = RuntimeDialogueSubtitleTranslations::Resolve(topicInfo, liveText);
				if (result.translated && !result.text.empty())
				{
					text = result.text;
				}
			});
		}
		g_showSubtitle(manager, speaker, text, topicInfo, forceDisplay);
	}

	void install()
	{
		REL::Relocation<std::uintptr_t> target{ kShowSubtitleID };

		// If another plugin has already hooked this function (the first byte is a
		// jmp instruction), skip installation to avoid trampoline chain breakage.
		// po3_FloatingSubtitlesF4 is a known concurrent hooker.
		if (const auto bytes = reinterpret_cast<const std::uint8_t*>(target.address());
			bytes[0] == 0xE9 || bytes[0] == 0xEB || bytes[0] == 0xFF)
		{
			REX::WARN("{} skipped SubtitleManager::ShowSubtitle hook; another plugin has already installed a hook at {:X}.",
				Plugin::NAME, target.address());
			return;
		}

		const auto result = RuntimePrologueHook::InstallJump(
			target.address(),
			reinterpret_cast<std::uintptr_t>(showSubtitleThunk),
			kMaxPatchBytes);
		if (!result.installed)
		{
			REX::ERROR("{} skipped SubtitleManager::ShowSubtitle hook; unsupported prologue bytes: {}",
				Plugin::NAME, result.prologueBytes);
			return;
		}

		g_showSubtitle = reinterpret_cast<ShowSubtitleFunc*>(result.original);
		REX::INFO("{} installed SubtitleManager::ShowSubtitle hook at {:X} using Address Library ID {}.",
			Plugin::NAME, target.address(), kShowSubtitleID.id());
	}
}

namespace RuntimeDialogueSubtitleHook
{
	void Install()
	{
		bool expected = false;
		if (!g_installed.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
		{
			return;
		}
		install();
	}
}

} // namespace Runtime111191
