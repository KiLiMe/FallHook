// AI CONTEXT: Resolves 1.10.163 XDI option candidates to source-free translated option text.
// Depends on RuntimeDialogueChoiceContext and RuntimeXdiDialogueOptionTranslations.
// Runtime scope is XDI DialogueMenu option selection only.
// Version-specific logic: follows 1.10.163 TESTopicInfo::dataInfo chains captured by dialogue hooks.
// Source-free policy: chooses by INFO form identity; visible text and XML Source are never lookup keys.
#pragma once

#include "110163/RuntimeXdiDialogueOptionTranslations.h"

#include <cstdint>

namespace RE
{
	class TESTopicInfo;
}

namespace RuntimeXdiDialogueOptionResolver
{
	struct ResolvedOption
	{
		RuntimeXdiDialogueOptionTranslations::OptionText text;
		RE::TESTopicInfo* info{ nullptr };
		std::uint32_t candidate0{ 0 };
		std::uint32_t candidate1{ 0 };
	};

	[[nodiscard]] bool HasText(const ResolvedOption& resolved);
	[[nodiscard]] ResolvedOption ResolveInfo(RE::TESTopicInfo* info);
	[[nodiscard]] ResolvedOption ResolveCapturedPrompt(std::uint32_t optionID);
}
