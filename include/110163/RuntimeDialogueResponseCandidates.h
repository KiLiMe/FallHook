// AI CONTEXT: Collects source-free dialogue response candidate INFO owners for 1.10.163.
// Depends on CommonLibF4 TESTopic/TESTopicInfo/TESResponse layout only.
// Runtime scope is Fallout 4 1.10.163 DialogueResponse construction cache misses.
// Version-specific logic: reads 1.10.163 topic info arrays and response linked lists.
// Source-free policy: returns runtime form/response identity only; never reads visible/source text.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace RE
{
	class TESResponse;
	class TESTopic;
	class TESTopicInfo;
}

namespace RuntimeDialogueResponseCandidates
{
	constexpr std::size_t kMaxCandidateCount{ 256 };

	struct TopicInfoCandidates
	{
		std::array<RE::TESTopicInfo*, kMaxCandidateCount> values{};
		std::size_t count{ 0 };
	};

	struct ResponseOwner
	{
		RE::TESTopicInfo* info{ nullptr };
		std::uint32_t ordinal{ 0xFFFFFFFFu };
	};

	[[nodiscard]] TopicInfoCandidates Make(RE::TESTopic* topic, RE::TESTopicInfo* topicInfo);
	[[nodiscard]] std::optional<ResponseOwner> FindOwner(
		const TopicInfoCandidates& candidates,
		const RE::TESResponse* response);
}
