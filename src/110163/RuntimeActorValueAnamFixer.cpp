// AI CONTEXT: Writes source-free AVIF:ANAM text into ActorValueInfo::abbreviation.
// Depends on RuntimeTextStringAssign and a verified Fallout 4 1.10.163 ActorValueInfo ANAM data slot.
// Runtime scope is Fallout 4 1.10.163 AVIF ActorValueInfo records for ANAM only.
// Version-specific logic: Fallout 4 1.10.163 AVIF ANAM offset 0x160.
// Source-free policy: verifies resolved AVIF form identity and writes destination text only.
#include "PCH.h"

#include "110163/RuntimeActorValueAnamFixer.h"

#include "RuntimeApplySettings.h"
#include "110163/RuntimeTextStringAssign.h"

#include <cstddef>

namespace
{
	constexpr std::ptrdiff_t kActorValueAnamOffset = 0x160;

	[[nodiscard]] RE::BGSLocalizedString* anamSlot(RE::ActorValueInfo* actorValue) noexcept
	{
		auto* bytes = reinterpret_cast<std::byte*>(actorValue);
		return reinterpret_cast<RE::BGSLocalizedString*>(bytes + kActorValueAnamOffset);
	}

	[[nodiscard]] const RE::BGSLocalizedString* anamSlot(const RE::ActorValueInfo* actorValue) noexcept
	{
		const auto* bytes = reinterpret_cast<const std::byte*>(actorValue);
		return reinterpret_cast<const RE::BGSLocalizedString*>(bytes + kActorValueAnamOffset);
	}

	void trace(
		const RuntimeApplySettings::Values& settings,
		std::string_view stage,
		const SourceFreeTranslationData& data,
		const RE::TESForm* form,
		const RE::ActorValueInfo* actorValue)
	{
		if (!settings.TraceEnabled())
		{
			return;
		}

		const auto* slot = actorValue ? anamSlot(actorValue) : nullptr;
		REX::INFO(
			"{} avif-anam trace stage={} formPtr={} avifPtr={} form={:08X} formType={} dataForm={:08X} dataEditor={} index={} sid={} textLen={} slotOffset=0x{:X} slotPtr={}",
			Plugin::NAME,
			stage,
			static_cast<const void*>(form),
			static_cast<const void*>(actorValue),
			form ? form->formID : 0,
			form ? form->GetFormTypeString() : "",
			data.formID.value_or(0),
			data.editorID.value_or(""),
			data.index.value_or(0xFFFFFFFFu),
			data.stringID.value_or(0xFFFFFFFFu),
			data.replacerText.size(),
			kActorValueAnamOffset,
			static_cast<const void*>(slot));
	}
}

namespace RuntimeActorValueAnamFixer
{
	bool Apply(RE::TESForm* form, const SourceFreeTranslationData& data)
	{
		const auto settings = RuntimeApplySettings::Load();
		trace(settings, "entry", data, form, nullptr);
		if (data.translationType != TranslationType::kActorValueAbbreviation || data.replacerText.empty())
		{
			trace(settings, "skip-input", data, form, nullptr);
			return false;
		}
		if (!form)
		{
			trace(settings, "skip-null-form", data, nullptr, nullptr);
			return false;
		}
		if (form->GetFormType() != RE::ENUM_FORM_ID::kAVIF)
		{
			trace(settings, "skip-not-avif", data, form, nullptr);
			return false;
		}

		auto* actorValue = form->As<RE::ActorValueInfo>();
		trace(settings, actorValue ? "cast-ok" : "cast-null", data, form, actorValue);
		if (!actorValue)
		{
			return false;
		}

		auto* abbreviation = anamSlot(actorValue);
		trace(settings, "assign-plain-begin", data, form, actorValue);
		RuntimeTextStringAssign::AssignPlainLocalized(*abbreviation, data.replacerText);
		trace(settings, "assign-ok", data, form, actorValue);
		return true;
	}
}
