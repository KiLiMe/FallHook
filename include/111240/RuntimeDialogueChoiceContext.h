// AI CONTEXT: Shares captured 1.11.240 dialogue choice identities between button and XDI hooks.
// Depends on RuntimeDialogueChoiceTranslations for the source-free form identity payload.
// Runtime scope is Fallout 4 1.11.240 player dialogue choices only.
// Version-specific logic: stores TESTopicInfo pointers captured from the verified 1.11.240 dialogue hook.
// Source-free policy: stores INFO/DIAL form identity and runtime pointers only; visible text is never a key.
#pragma once

#include "111240/RuntimeDialogueChoiceTranslations.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace RE
{
	class TESTopicInfo;
}

namespace Runtime111240::RuntimeDialogueChoiceContext
{
	constexpr std::size_t kButtonCount{ 4 };
	constexpr std::size_t kContextsPerButton{ 4 };

	struct CapturedContext
	{
		RuntimeDialogueChoiceTranslations::ChoiceContext choice;
		std::array<RE::TESTopicInfo*, RuntimeDialogueChoiceTranslations::kMaxChoiceInfos> infos{};
		std::size_t infoCount{ 0 };
	};

	[[nodiscard]] CapturedContext Make(RE::TESTopicInfo* info, std::uint32_t responseType);
	void Remember(std::uint32_t responseType, const CapturedContext& context);
	[[nodiscard]] std::array<CapturedContext, kContextsPerButton> Snapshot(std::uint32_t responseType);
}
