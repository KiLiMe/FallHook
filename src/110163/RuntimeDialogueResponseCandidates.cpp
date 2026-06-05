// AI CONTEXT: Implements 1.10.163 topic-local INFO candidate discovery for DialogueResponse translation.
// Depends on RuntimeDialogueResponseCandidates and CommonLibF4 TESTopic/TESTopicInfo/TESResponse layout.
// Runtime scope is cache-miss identity discovery during 1.10.163 DialogueResponse construction.
// Version-specific logic: walks TESTopic::topicInfos and TESTopicInfo::responses for Fallout 4 1.10.163.
// Source-free policy: uses pointers/form ownership only; no source/visible text matching.
#include "PCH.h"

#include "110163/RuntimeDialogueResponseCandidates.h"

#include "RE/T/TESResponse.h"
#include "RE/T/TESTopic.h"
#include "RE/T/TESTopicInfo.h"

namespace
{
	constexpr std::uint32_t kMaxResponses{ 128 };

	void pushInfo(RuntimeDialogueResponseCandidates::TopicInfoCandidates& candidates, RE::TESTopicInfo* info)
	{
		if (!info)
		{
			return;
		}
		for (std::size_t i = 0; i < candidates.count; ++i)
		{
			if (candidates.values[i] == info)
			{
				return;
			}
		}
		if (candidates.count < candidates.values.size())
		{
			candidates.values[candidates.count++] = info;
		}
	}

	void appendInfoChain(RuntimeDialogueResponseCandidates::TopicInfoCandidates& candidates, RE::TESTopicInfo* info)
	{
		for (auto* current = info; current && candidates.count < candidates.values.size(); current = current->dataInfo)
		{
			pushInfo(candidates, current);
		}
	}

	void appendTopicInfos(RuntimeDialogueResponseCandidates::TopicInfoCandidates& candidates, RE::TESTopic* topic)
	{
		if (!topic || !topic->topicInfos)
		{
			return;
		}
		for (std::uint32_t i = 0; i < topic->numTopicInfos && candidates.count < candidates.values.size(); ++i)
		{
			pushInfo(candidates, topic->topicInfos[i]);
		}
	}
}

namespace RuntimeDialogueResponseCandidates
{
	TopicInfoCandidates Make(RE::TESTopic* topic, RE::TESTopicInfo* topicInfo)
	{
		TopicInfoCandidates candidates;
		if (!topic && topicInfo)
		{
			topic = topicInfo->parentTopic;
		}
		auto* responseInfo = topicInfo;
		for (std::size_t depth = 0; responseInfo && responseInfo->dataInfo && depth < kMaxCandidateCount; ++depth)
		{
			responseInfo = responseInfo->dataInfo;
		}
		pushInfo(candidates, responseInfo);
		appendInfoChain(candidates, topicInfo);
		appendInfoChain(candidates, responseInfo);
		appendTopicInfos(candidates, topic);
		return candidates;
	}

	std::optional<ResponseOwner> FindOwner(const TopicInfoCandidates& candidates, const RE::TESResponse* response)
	{
		if (!response)
		{
			return std::nullopt;
		}
		for (std::size_t i = 0; i < candidates.count; ++i)
		{
			const auto* candidate = candidates.values[i];
			std::uint32_t ordinal = 0;
			for (auto* current = candidate ? candidate->responses.head : nullptr;
				 current && ordinal < kMaxResponses;
				 current = current->pNext, ++ordinal)
			{
				if (current == response)
				{
					return ResponseOwner{ candidates.values[i], ordinal };
				}
			}
		}
		return std::nullopt;
	}
}
