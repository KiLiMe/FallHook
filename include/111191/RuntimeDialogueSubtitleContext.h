// AI CONTEXT: Carries resolved INFO:NAM1 response identity from construction to subtitle display.
// Depends on CommonLibF4 fixed strings and TESTopicInfo identity.
// Runtime scope is Fallout 4 1.11.191 dialogue response construction and subtitle handoff.
// Version-specific logic: uses 1.11.191 TESTopicInfo form identity and pooled BSFixedString pointers.
// Source-free policy: keys by INFO/speaker runtime identity and string-pool pointer; never matches visible/source text.
#pragma once

#include "RE/B/BSFixedString.h"

#include <cstdint>

namespace RE
{
	class TESObjectREFR;
	class TESTopicInfo;
}

namespace Runtime111191::RuntimeDialogueSubtitleContext
{
	struct LookupResult
	{
		RE::BSFixedStringCS text;
		std::uint32_t ordinal{ 0xFFFFFFFFu };
		std::uint32_t responseID{ 0 };
		bool translated{ false };
	};

	void Remember(
		RE::TESTopicInfo* topicInfo,
		RE::TESObjectREFR* speaker,
		const RE::BSFixedStringCS& rawText,
		const RE::BSFixedStringCS& destination,
		std::uint32_t ordinal,
		std::uint32_t responseID);
	[[nodiscard]] LookupResult Resolve(
		RE::TESTopicInfo* topicInfo,
		RE::TESObjectREFR* speaker,
		const RE::BSFixedStringCS& rawText);
	void Clear();
}
