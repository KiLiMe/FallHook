// AI CONTEXT: Owns Fallout 4 1.11.191 hook installation and message-driven runtime callbacks.
// Depends on all active 111191 hook modules plus shared settings, logging, and watchdog helpers.
// Runtime assumptions: selected only after F4SE reports Fallout 4 runtime 1.11.191.
// Version-specific logic: yes; all hook installs here target 1.11.191-compatible modules.
// Source-free policy: preserves source-free XML identity; InGameText TXT remains the approved source-key exception.
#include "PCH.h"

#include "111191/RuntimeModule111191.h"

#include "111191/RuntimeActivationContextHook.h"
#include "111191/RuntimeDescriptionHook.h"
#include "111191/RuntimeDialogueButtonHook.h"
#include "111191/RuntimeDialogueResponseHook.h"
#include "111191/RuntimeDialogueSubtitleHook.h"
#include "111191/RuntimeFullNameLoadHook.h"
#include "111191/RuntimeHudRolloverHook.h"
#include "RuntimeInGameTextDictionary.h"
#include "111191/RuntimeInGameTextHook.h"
#include "RuntimeInGameTextLog.h"
#include "RuntimeInGameTextSettings.h"
#include "RuntimeLoadWatchdog.h"
#include "111191/RuntimePipboyLogHook.h"
#include "111191/RuntimeQuestJournalTextHook.h"
#include "111191/RuntimeTextManager.h"
#include "111191/RuntimeXdiDialogueMenuHook.h"
#include "RuntimeStringOverlay.h"
#include "111191/RuntimeTextStringAssign.h"
#include "111191/RuntimeLocalizedStringID.h"
#include "RE/T/TESDataHandler.h"
#include "RE/T/TESFullName.h"
#include "RE/T/TESForm.h"

namespace Runtime111191
{
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

			// Apply overlay translations to all loaded TESFullName forms.
			// At boot time the localized strings were raw binary (no <ID=...>),
			// so the FullName load hook could not match sIDs against overlay.
			// By kGameDataReady the strings are resolved; iterate all target
			// form types and apply any overlay match.
			{
				auto* handler = RE::TESDataHandler::GetSingleton();
				if (handler && RuntimeStringOverlay::Count() > 0)
				{
					std::size_t applied = 0;
					const auto applyOverlay = [&](auto* form) {
						if (!form)
							return;
						auto* fullName = form->As<RE::TESFullName>();
						if (!fullName)
							return;
						const auto stringID = Runtime111191::RuntimeLocalizedStringID::Read(fullName->fullName);
						if (!stringID)
							return;
						const auto* overlayText = RuntimeStringOverlay::Lookup(*stringID);
						if (!overlayText || overlayText->empty())
							return;
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
			static_cast<void>(RuntimeInGameTextDictionary::LoadFromDisk(inGameTextSettings.enable));
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "F4SEPluginLoad RuntimeInGameTextHook::Install", 0.0 };
			RuntimeInGameTextHook::Install(inGameTextSettings);
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "F4SEPluginLoad RuntimePipboyLogHook::Install", 0.0 };
			RuntimePipboyLogHook::Install();
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "F4SEPluginLoad RuntimeQuestJournalTextHook::Install", 0.0 };
			RuntimeQuestJournalTextHook::Install();
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
			RuntimeLoadWatchdog::ScopedPhase phase{ "F4SEPluginLoad RuntimeActivationContextHook::Install", 0.0 };
			RuntimeActivationContextHook::Install();
		}
		{
			RuntimeLoadWatchdog::ScopedPhase phase{ "F4SEPluginLoad RuntimeHudRolloverHook::Install", 0.0 };
			RuntimeHudRolloverHook::Install();
		}
		REX::INFO("{} item-name runtime uses FULL load hook, direct FULL mutation, global template mutation, and INNR data mutation.", Plugin::NAME);
		REX::INFO(
			"{} InGameText TXT source-key exception is enabled={} logRaw={}.",
			Plugin::NAME,
			inGameTextSettings.enable,
			inGameTextSettings.logRaw);
		REX::INFO("{} AVIF:ANAM const apply is enabled with staged trace.", Plugin::NAME);
		REX::INFO("{} QUST:CNAM journal hook is enabled for 1.11.191 with TESQuestStageItem::GetLogEntry.", Plugin::NAME);
		REX::INFO("{} BPTD:BPTN body-part names use source-free plugin BPND.type slots and const data mutation.", Plugin::NAME);
		REX::INFO("{} INFO:RNAM dialogue choices use BGSSceneActionPlayerDialogue context plus DialogueMenuUtils::SetButtonText only; DIAL:FULL remains data-level and INFO:NAM1 response hooks capture identity while SubtitleManager performs spoken-text replacement.", Plugin::NAME);
		REX::INFO("{} XDI DialogueMenu choices use a source-free GetDialogueOptions option wrapper when XDI is present.", Plugin::NAME);
		REX::INFO("{} HUDRollover action replacement uses moved ShowRollover ID 2221994, ACTI/FURN primary slot +8, PERK EPF2 secondary slot +10, and parameter copy ID 2222029.", Plugin::NAME);
		REX::INFO("{} REGN:RDMP region map-data mutation uses TESRegionDataList::Find ID 2196228.", Plugin::NAME);
		REX::INFO("{} source-free const text mutation and runtime text hooks enabled.", Plugin::NAME);
		return true;
	}

	void Shutdown()
	{}

	// The AE module was written and verified against 1.11.191. Newer AE patches
	// reuse it unchanged because every hook uses Address Library REL::ID lookups.
	// Add a runtime here (not in the dispatcher) when a new AE patch needs no
	// hook changes.
	constexpr REL::Version kAcceptedRuntimes[] = {
		RuntimeVersionDispatcher::kRuntime111240
	};

	const RuntimeVersionDispatcher::Module kModule{
		.name = "111191",
		.runtime = RuntimeVersionDispatcher::kRuntime111191,
		.acceptedRuntimes = kAcceptedRuntimes,
		.Load = Load,
		.HandleMessage = HandleMessage,
		.Shutdown = Shutdown
	};
}

namespace RuntimeModule111191
{
	const RuntimeVersionDispatcher::Module& GetModule() noexcept
	{
		return kModule;
	}
}

} // namespace Runtime111191
