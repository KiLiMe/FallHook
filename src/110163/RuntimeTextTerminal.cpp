// AI CONTEXT: Applies destination text to indexed terminal body/menu/header slots.
// Depends on RuntimeApplySettings, RuntimeLocalizedStringID, RuntimeTextTerminal, RuntimeTextStringAssign, and CommonLibF4 BGSTerminal.
// Runtime scope is Fallout 4 1.10.163 terminal text mutation.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: selects terminal slots by source-free string ID or subrecord ordinal; never original text.
#include "PCH.h"

#include "RuntimeApplySettings.h"
#include "110163/RuntimeLocalizedStringID.h"
#include "110163/RuntimeTextStringAssign.h"
#include "110163/RuntimeTextTerminal.h"

#include <atomic>

namespace
{
	struct TerminalSlotIdentity
	{
		std::uint32_t fieldIndex{ 0 };
		std::uint32_t menuIndex{ 0 };
		std::uint16_t itemID{ 0 };
		std::optional<std::uint32_t> stringID;
	};

	constexpr std::uint32_t kTraceLimit{ 512 };
	std::atomic_uint32_t g_traceLines{ 0 };

	bool isTerminalResultTextItem(const RE::BGSTerminal::MenuItem& item)
	{
		using Flag = RE::BGSTerminal::MenuItem::Flag;
		const auto rawFlags = static_cast<std::uint8_t>(item.flags.underlying());
		const auto rawTextValue = static_cast<std::uint8_t>(std::to_underlying(Flag::kText));
		constexpr std::uint8_t rawTextBit = 1u << std::to_underlying(Flag::kText);
		return item.selectionResult.displayText && (rawFlags == rawTextValue || (rawFlags & rawTextBit) != 0);
	}

	bool terminalSlotMatches(const SourceFreeTranslationData& data, const TerminalSlotIdentity& slot)
	{
		if (data.stringID && slot.stringID && *data.stringID == *slot.stringID)
		{
			return true;
		}
		if (!data.index)
		{
			return slot.fieldIndex == 0 && slot.menuIndex == 0 && slot.itemID == 0;
		}
		return *data.index == slot.fieldIndex;
	}

	void traceTerminalSlot(
		const RuntimeApplySettings::Values& settings,
		std::string_view stage,
		const RE::BGSTerminal& terminal,
		const SourceFreeTranslationData& data,
		const TerminalSlotIdentity& slot)
	{
		if (!settings.TraceEnabled())
		{
			return;
		}
		if (g_traceLines.fetch_add(1, std::memory_order_relaxed) >= kTraceLimit)
		{
			return;
		}

		REX::INFO(
			"{} terminal trace stage={} form={:08X} type={} editor={} targetIndex={} targetSid={:08X} fieldIndex={} menuIndex={} itemID={} slotSid={:08X}",
			Plugin::NAME,
			stage,
			static_cast<std::uint32_t>(terminal.formID),
			static_cast<std::uint8_t>(data.translationType),
			data.editorID.value_or(""),
			data.index.value_or(0xFFFFFFFFu),
			data.stringID.value_or(0xFFFFFFFFu),
			slot.fieldIndex,
			slot.menuIndex,
			slot.itemID,
			slot.stringID.value_or(0xFFFFFFFFu));
	}

	template <class F>
	bool forEachPreferredTerminalText(RE::BGSTerminal& terminal, TranslationType type, F func)
	{
		if (type == TranslationType::kTerminalBodyText)
		{
			for (std::uint32_t index = 0; auto& bodyText : terminal.bodyTextItems)
			{
				if (func(bodyText.itemText, index, index, 0))
				{
					return true;
				}
				++index;
			}
			return false;
		}
		if (type == TranslationType::kTerminalHeaderText)
		{
			return func(terminal.headerTextOverride, 0, 0, 0);
		}
		if (type == TranslationType::kTerminalWelcomeText)
		{
			return func(terminal.welcomeText, 0, 0, 0);
		}

		std::uint32_t menuIndex = 0;
		std::uint32_t fieldIndex = 0;
		for (auto& item : terminal.menuItems)
		{
			const auto currentMenuIndex = menuIndex++;
			RE::BGSLocalizedString* target = nullptr;
			if (type == TranslationType::kTerminalItemText)
			{
				target = std::addressof(item.itemText);
			}
			else if (type == TranslationType::kTerminalResponseText)
			{
				target = std::addressof(item.responseText);
			}
			else if (type == TranslationType::kTerminalResultText && isTerminalResultTextItem(item))
			{
				target = item.selectionResult.displayText;
			}

			if (target && func(*target, fieldIndex, currentMenuIndex, item.id))
			{
				return true;
			}
			if (target)
			{
				++fieldIndex;
			}
		}
		return false;
	}
}

namespace RuntimeTextTerminal
{
	bool ApplyTerminalText(RE::TESForm* form, const SourceFreeTranslationData& data)
	{
		auto* terminal = form ? form->As<RE::BGSTerminal>() : nullptr;
		if (!terminal)
		{
			return false;
		}

		const auto settings = RuntimeApplySettings::Load();
		return forEachPreferredTerminalText(*terminal, data.translationType, [&](RE::BGSLocalizedString& value, std::uint32_t fieldIndex, std::uint32_t menuIndex, std::uint16_t itemID) {
			const TerminalSlotIdentity slot{
				.fieldIndex = fieldIndex,
				.menuIndex = menuIndex,
				.itemID = itemID,
				.stringID = RuntimeLocalizedStringID::Read(value)
			};
			if (!terminalSlotMatches(data, slot))
			{
				traceTerminalSlot(settings, "skip-slot", *terminal, data, slot);
				return false;
			}

			traceTerminalSlot(settings, "apply-slot", *terminal, data, slot);
			RuntimeTextStringAssign::AssignLocalized(value, data.replacerText);
			return true;
		});
	}
}
