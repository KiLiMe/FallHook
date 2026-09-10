// AI CONTEXT: Owns Fallout 4 1.10.163 hook installation and message-driven runtime callbacks.
// Depends on all active 110163 hook modules plus shared settings, logging, and watchdog helpers.
// Runtime assumptions: selected only after F4SE reports Fallout 4 runtime 1.10.163.
// Version-specific logic: yes; all hook installs here target 1.10.163-compatible modules.
// Source-free policy: preserves source-free XML identity; InGameText TXT remains the approved source-key exception.
#include "PCH.h"

#include "110163/RuntimeModule110163.h"

#include "110163/RuntimeActivationContextHook.h"
#include "110163/RuntimeDescriptionHook.h"
#include "110163/RuntimeDialogueButtonHook.h"
#include "110163/RuntimeDialogueResponseHook.h"
#include "110163/RuntimeDialogueSubtitleHook.h"
#include "110163/RuntimeFullNameLoadHook.h"
#include "110163/RuntimeHudRolloverHook.h"
#include "RuntimeInGameTextDictionary.h"
#include "110163/RuntimeInGameTextHook.h"
#include "RuntimeInGameTextLog.h"
#include "RuntimeInGameTextSettings.h"
#include "RuntimeLoadWatchdog.h"
#include "110163/RuntimePipboyLogHook.h"
#include "110163/RuntimeQuestJournalTextHook.h"
#include "110163/RuntimeTextManager.h"
#include "110163/RuntimeXdiDialogueMenuHook.h"
#include "RuntimeStringOverlay.h"
#include "110163/RuntimeTextStringAssign.h"
#include "110163/RuntimeLocalizedStringID.h"
#include "RE/T/TESDataHandler.h"
#include "RE/T/TESFullName.h"

namespace
{
	void HandleMessage(F4SE::MessagingInterface::Message* message)
	{
		if (!message)
		{
			return;
		}

		if (message->type == F4SE::MessagingInterface::kGameDataReady)
		{
			RuntimeTextManager::ApplyBootCatalogOnce();

			{
				auto* handler = RE::TESDataHandler::GetSingleton();
				if (handler && RuntimeStringOverlay::Count() > 0)
				{
					std::size_t applied = 0;
					const auto applyOverlay = [&](auto* form) {
						if (!form) return;
						auto* fullName = form->As<RE::TESFullName>();
						if (!fullName) return;
						const auto stringID = RuntimeLocalizedStringID::Read(fullName->fullName);
						if (!stringID) return;
						const auto* overlayText = RuntimeStringOverlay::Lookup(*stringID);
						if (!overlayText || overlayText->empty()) return;
						RuntimeTextStringAssign::AssignPlainLocalized(fullName->fullName, *overlayText);
						++applied;
					};

					for (auto* weap : handler->GetFormArray<RE::TESObjectWEAP>())
						applyOverlay(weap);
					for (auto* armo : handler->GetFormArray<RE::TESObjectARMO>())
						applyOverlay(armo);
					for (auto* misc : handler->GetFormArray<RE::TESObjectMISC>())
						applyOverlay(misc);
					for (auto* npc : handler->GetFormArray<RE::TESNPC>())
						applyOverlay(npc);
					for (auto* book : handler->GetFormArray<RE::TESObjectBOOK>())
						applyOverlay(book);
					for (auto* ammo : handler->GetFormArray<RE::TESAmmo>())
						applyOverlay(ammo);
					for (auto* keym : handler->GetFormArray<RE::TESKey>())
						applyOverlay(keym);
					for (auto* slgm : handler->GetFormArray<RE::TESSoulGem>())
						applyOverlay(slgm);
					for (auto* spel : handler->GetFormArray<RE::SpellItem>())
						applyOverlay(spel);
					for (auto* ench : handler->GetFormArray<RE::EnchantmentItem>())
						applyOverlay(ench);
					for (auto* furn : handler->GetFormArray<RE::TESFurniture>())
						applyOverlay(furn);
					for (auto* cont : handler->GetFormArray<RE::TESObjectCONT>())
						applyOverlay(cont);
					for (auto* door : handler->GetFormArray<RE::TESObjectDOOR>())
						applyOverlay(door);
					for (auto* ligh : handler->GetFormArray<RE::TESObjectLIGH>())
						applyOverlay(ligh);

					REX::INFO("{} applied {} overlay translation(s) to FullName forms at GameDataReady.",
						Plugin::NAME, applied);
				}
			}
		}
		else if (message->type == F4SE::MessagingInterface::kNewGame ||
				 message->type == F4SE::MessagingInterface::kPostLoadGame)
		{
			RuntimeTextManager::RefreshAfterSaveLoad();
		}
	}

	bool Load(const F4SE::LoadInterface*)
	{
		const auto inGameTextSettings = RuntimeInGameTextSettings::Load();
		RuntimeInGameTextLog::Configure(inGameTextSettings.logRaw);

		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "F4SEPluginLoad InGameText TXT dictionary load", 0.0 };
			[[maybe_unused]] const auto inGameTextLoadStats = RuntimeInGameTextDictionary::LoadFromDisk(inGameTextSettings.enable);
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "F4SEPluginLoad RuntimeInGameTextHook::Install", 0.0 };
			RuntimeInGameTextHook::Install(inGameTextSettings);
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "F4SEPluginLoad RuntimeHudRolloverHook::Install", 0.0 };
			RuntimeHudRolloverHook::Install();
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "F4SEPluginLoad RuntimePipboyLogHook::Install", 0.0 };
			RuntimePipboyLogHook::Install();
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "F4SEPluginLoad RuntimeDescriptionHook::Install", 0.0 };
			RuntimeDescriptionHook::Install();
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "F4SEPluginLoad RuntimeDialogueResponseHook::Install", 0.0 };
			RuntimeDialogueResponseHook::Install();
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "F4SEPluginLoad RuntimeDialogueSubtitleHook::Install", 0.0 };
			RuntimeDialogueSubtitleHook::Install();
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "F4SEPluginLoad RuntimeDialogueButtonHook::Install", 0.0 };
			RuntimeDialogueButtonHook::Install();
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "F4SEPluginLoad RuntimeXdiDialogueMenuHook::Install", 0.0 };
			RuntimeXdiDialogueMenuHook::Install();
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "F4SEPluginLoad RuntimeFullNameLoadHook::Install", 0.0 };
			RuntimeFullNameLoadHook::Install();
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "F4SEPluginLoad RuntimeQuestJournalTextHook::Install", 0.0 };
			RuntimeQuestJournalTextHook::Install();
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "F4SEPluginLoad RuntimeActivationContextHook::Install", 0.0 };
			RuntimeActivationContextHook::Install();
		}

		REX::INFO("{} item-name runtime is data-only: FULL load, global template mutation, and INNR data mutation.", Plugin::NAME);
		REX::INFO(
			"{} InGameText TXT source-key exception is enabled={} logRaw={}.",
			Plugin::NAME,
			inGameTextSettings.enable,
			inGameTextSettings.logRaw);
		REX::INFO("{} AVIF:ANAM const apply is enabled with staged trace.", Plugin::NAME);
		REX::INFO("{} QUST:CNAM uses TESQuest::GetJournalTextForStageItem journal hook.", Plugin::NAME);
		REX::INFO("{} BPTD:BPTN body-part names use source-free plugin BPND.type slots and const data mutation.", Plugin::NAME);
		REX::INFO("{} INFO:RNAM dialogue choices use BGSSceneActionPlayerDialogue context plus DialogueMenuUtils::SetButtonText only; DIAL:FULL remains data-level and INFO:NAM1 response hooks capture identity while SubtitleManager performs the only spoken-text replacement.", Plugin::NAME);
		REX::INFO("{} XDI DialogueMenu choices use a source-free GetDialogueOptions option wrapper when XDI is present.", Plugin::NAME);
		REX::INFO("{} HUDRollover user-approved post-data exception replaces trap, furniture, and robot HACK actions from source-free maps.", Plugin::NAME);
		REX::INFO("{} source-free const text mutation and runtime text hooks enabled.", Plugin::NAME);
		return true;
	}

	void Shutdown()
	{}

	const RuntimeVersionDispatcher::Module kModule{
		.name = "110163",
		.runtime = RuntimeVersionDispatcher::kRuntime110163,
		// Owns 1.10.163 only; no other runtime shares this layout.
		.acceptedRuntimes = {},
		.Load = Load,
		.HandleMessage = HandleMessage,
		.Shutdown = Shutdown
	};
}

namespace RuntimeModule110163
{
	const RuntimeVersionDispatcher::Module& GetModule() noexcept
	{
		return kModule;
	}
}
