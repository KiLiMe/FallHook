// AI CONTEXT: Applies destination text to indexed list fields on resolved forms, including body-part names.
// Depends on MessageIconFormatter, RuntimeTextIndexedLists declarations, RuntimeTextStringAssign, and CommonLibF4 list types.
// Runtime scope is Fallout 4 1.10.163 indexed direct text mutation.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: selects targets by index/objective id only; MESG runtime text is a markup donor after lookup.
// Debug note: BPTD trace may print live target strings for diagnostics only; lookup never uses those strings.
// Exception note: MESG icon preservation is approved and must not be treated as source-exact lookup.
#include "PCH.h"

#include "MessageIconFormatter.h"
#include "RuntimeApplySettings.h"
#include "110163/RuntimeTextIndexedLists.h"
#include "110163/RuntimeTextStringAssign.h"

#include <atomic>

namespace
{
	constexpr std::uint32_t kBodyPartTraceLimit = 2048;
	constexpr std::uint32_t kUnsetIndex = 0xFFFFFFFFu;
	constexpr std::uint32_t kBodyPartSlotCount = 26;

	std::atomic_uint32_t g_bodyPartTraceLines{ 0 };

	[[nodiscard]] std::string_view localizedView(const RE::BGSLocalizedString& text) noexcept
	{
		return { text.c_str(), text.size() };
	}

	[[nodiscard]] std::string_view fixedView(const RE::BSFixedString& text) noexcept
	{
		return { text.c_str(), text.size() };
	}

	[[nodiscard]] const char* editorID(const RE::TESForm* form) noexcept
	{
		const auto* editor = form ? form->GetFormEditorID() : nullptr;
		return editor ? editor : "";
	}

	[[nodiscard]] bool shouldTraceBodyPart(const RuntimeApplySettings::Values& settings)
	{
		if (!settings.TraceEnabled())
		{
			return false;
		}

		const auto previous = g_bodyPartTraceLines.fetch_add(1, std::memory_order_relaxed);
		if (previous < kBodyPartTraceLimit)
		{
			return true;
		}
		if (previous == kBodyPartTraceLimit)
		{
			REX::INFO("{} body-part trace stage=limit-reached cap={}", Plugin::NAME, kBodyPartTraceLimit);
		}
		return false;
	}

	void traceBodyPartBase(
		const RuntimeApplySettings::Values& settings,
		std::string_view stage,
		const RE::TESForm* form,
		const SourceFreeTranslationData& data,
		const RE::BGSBodyPartData* bodyPartData,
		std::uint32_t nonNullParts = kUnsetIndex)
	{
		if (!shouldTraceBodyPart(settings))
		{
			return;
		}

		REX::INFO(
			"{} body-part trace stage={} form={:08X} formType={} editor={} bodyPtr={} xmlIndex={} sid={} destLen={} nonNullParts={}",
			Plugin::NAME,
			stage,
			form ? form->formID : 0,
			form ? form->GetFormTypeString() : "",
			editorID(form),
			reinterpret_cast<std::uintptr_t>(bodyPartData),
			data.index.value_or(kUnsetIndex),
			data.stringID.value_or(kUnsetIndex),
			data.replacerText.size(),
			nonNullParts);
	}

	void traceBodyPartSlot(
		const RuntimeApplySettings::Values& settings,
		std::string_view stage,
		const RE::TESForm* form,
		const SourceFreeTranslationData& data,
		const RE::BGSBodyPart* part,
		std::uint32_t slot,
		std::uint32_t dense)
	{
		if (!part || !shouldTraceBodyPart(settings))
		{
			return;
		}

		const auto current = localizedView(part->partName);
		REX::INFO(
			"{} body-part trace stage={} form={:08X} editor={} xmlIndex={} sid={} dense={} slot={} partPtr={} geom={} type={} flags={} toHit={} currentLen={} current=\"{}\" node=\"{}\" target=\"{}\" destLen={}",
			Plugin::NAME,
			stage,
			form ? form->formID : 0,
			editorID(form),
			data.index.value_or(kUnsetIndex),
			data.stringID.value_or(kUnsetIndex),
			dense,
			slot,
			reinterpret_cast<std::uintptr_t>(part),
			static_cast<std::uint32_t>(part->data.geometrySegmentIndex),
			static_cast<std::uint32_t>(part->data.type),
			static_cast<std::uint32_t>(part->data.flags),
			static_cast<std::uint32_t>(part->data.toHitChance),
			current.size(),
			current,
			fixedView(part->nodeName),
			fixedView(part->targetName),
			data.replacerText.size());
	}
}

namespace RuntimeTextIndexedLists
{
	bool ApplyMessageBoxButton(RE::TESForm* form, const SourceFreeTranslationData& data)
	{
		auto* message = form ? form->As<RE::BGSMessage>() : nullptr;
		if (!message || !data.index)
		{
			return false;
		}

		std::uint32_t pos = 0;
		for (auto* button : message->buttonList)
		{
			if (button && pos == *data.index)
			{
				const auto formatted = MessageIconFormatter::ApplyRuntimeControlMarkup(button->text.c_str(), data.replacerText);
				RuntimeTextStringAssign::AssignLocalized(button->text, formatted.text);
				return true;
			}
			if (button)
			{
				++pos;
			}
		}
		return false;
	}

	bool ApplyQuestObjective(RE::TESForm* form, const SourceFreeTranslationData& data)
	{
		auto* quest = form ? form->As<RE::TESQuest>() : nullptr;
		if (!quest || !data.index)
		{
			return false;
		}

		for (auto* objective : quest->objectives)
		{
			if (objective && objective->index == *data.index)
			{
				RuntimeTextStringAssign::AssignLocalized(objective->displayText, data.replacerText);
				return true;
			}
		}

		std::uint32_t pos = 0;
		for (auto* objective : quest->objectives)
		{
			if (objective && pos++ == *data.index)
			{
				RuntimeTextStringAssign::AssignLocalized(objective->displayText, data.replacerText);
				return true;
			}
		}
		return false;
	}

	bool ApplyBodyPartName(RE::TESForm* form, const SourceFreeTranslationData& data)
	{
		const auto settings = RuntimeApplySettings::Load();
		auto* bodyPartData = form ? form->As<RE::BGSBodyPartData>() : nullptr;
		if (!form)
		{
			traceBodyPartBase(settings, "skip-null-form", form, data, bodyPartData);
			return false;
		}
		if (!bodyPartData)
		{
			traceBodyPartBase(settings, "skip-invalid-form", form, data, bodyPartData);
			return false;
		}
		if (!data.index)
		{
			traceBodyPartBase(settings, "skip-missing-index", form, data, bodyPartData);
			return false;
		}

		traceBodyPartBase(settings, "apply-begin", form, data, bodyPartData);
		const auto slot = *data.index;
		if (slot >= kBodyPartSlotCount)
		{
			traceBodyPartBase(settings, "skip-slot-out-of-range", form, data, bodyPartData, slot);
			return false;
		}

		auto* part = bodyPartData->partArray[slot];
		if (!part)
		{
			traceBodyPartBase(settings, "skip-empty-slot", form, data, bodyPartData, slot);
			return false;
		}

		traceBodyPartSlot(settings, "assign-begin", form, data, part, slot, slot);
		RuntimeTextStringAssign::AssignLocalized(part->partName, data.replacerText);
		traceBodyPartSlot(settings, "assign-end", form, data, part, slot, slot);
		return true;
	}

	bool ApplyFactionRankTitle(RE::TESForm* form, const SourceFreeTranslationData& data, bool female)
	{
		auto* faction = form ? form->As<RE::TESFaction>() : nullptr;
		if (!faction || !data.index)
		{
			return false;
		}

		std::uint32_t pos = 0;
		for (auto* rank : faction->rankDataList)
		{
			if (rank && pos++ == *data.index)
			{
				RuntimeTextStringAssign::AssignLocalized(female ? rank->femaleRankTitle : rank->maleRankTitle, data.replacerText);
				return true;
			}
		}
		return false;
	}
}
