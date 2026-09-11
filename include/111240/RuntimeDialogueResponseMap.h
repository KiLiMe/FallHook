// AI CONTEXT: Declares source-free INFO:NAM1 response target maps and lookup helpers.
// Depends on TranslationCatalog target text, CommonLibF4 fixed strings, and 111240 response candidates.
// Runtime scope is Fallout 4 1.11.240 dialogue response identity lookup.
// Version-specific logic: uses 111240 TESTopicInfo candidate identity supplied by the owning runtime module.
// Source-free policy: lookup uses INFO form, response sID, response ID, and REC index only; never Source text.
#pragma once

#include "111240/RuntimeDialogueResponseCandidates.h"

#include "RE/B/BSFixedString.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace RE
{
	class TESTopicInfo;
}

namespace Runtime111240::RuntimeDialogueResponseMap
{
	struct Target
	{
		std::string text;
		RE::BSFixedStringCS fixedText;
	};

	struct Targets
	{
		std::unordered_map<std::uint32_t, Target> bySID;
		std::unordered_map<std::uint32_t, Target> byIndex;
		std::unordered_map<std::uint32_t, Target> byResponseID;
		Target defaultTarget;
		bool hasDefault{ false };
		bool defaultAmbiguous{ false };
	};

	struct Snapshot
	{
		std::unordered_map<std::uint32_t, Targets> byInfoForm;
		std::unordered_map<std::uint32_t, Target> byUniqueSID;
		std::uint64_t generation{ 0 };
		bool traceEnabled{ false };
	};

	[[nodiscard]] Target MakeTarget(std::string text);
	void AddUniqueSID(Snapshot& snapshot, std::uint32_t sid, const Target& target);
	void AddDefaultTarget(Targets& targets, const Target& target);
	void AddResponseIDTarget(Targets& targets, std::uint32_t id, const Target& target);
	void AddIndexCandidate(std::array<std::uint32_t, 5>& indexes, std::size_t& count, std::uint32_t value);
	[[nodiscard]] std::array<std::uint32_t, 5> IndexCandidates(
		std::uint32_t ordinal,
		std::uint32_t id,
		std::size_t& count);
	[[nodiscard]] const Target* LookupBySID(
		const Snapshot& snapshot,
		const RuntimeDialogueResponseCandidates::TopicInfoCandidates& candidates,
		std::uint32_t sid);
	[[nodiscard]] const Target* LookupFormSID(
		const Snapshot& snapshot,
		std::uint32_t formID,
		std::uint32_t sid);
	[[nodiscard]] const Target* LookupInfoSID(
		const Snapshot& snapshot,
		const RE::TESTopicInfo* info,
		std::uint32_t sid);
	[[nodiscard]] const Target* LookupInfoDefault(
		const Snapshot& snapshot,
		const RE::TESTopicInfo* info);
	[[nodiscard]] const Target* LookupByResponseID(
		const Snapshot& snapshot,
		const RuntimeDialogueResponseCandidates::TopicInfoCandidates& candidates,
		std::uint32_t id);
	[[nodiscard]] const Target* LookupInfoIndex(
		const Snapshot& snapshot,
		const RE::TESTopicInfo* info,
		std::uint32_t index);
	[[nodiscard]] const Target* LookupInfoResponseID(
		const Snapshot& snapshot,
		const RE::TESTopicInfo* info,
		std::uint32_t id);
	[[nodiscard]] const Target* LookupByIndex(
		const Snapshot& snapshot,
		const RuntimeDialogueResponseCandidates::TopicInfoCandidates& candidates,
		const std::array<std::uint32_t, 5>& indexes,
		std::size_t count);
}
