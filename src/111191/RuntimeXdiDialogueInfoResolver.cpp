// AI CONTEXT: Implements 1.11.191 XDI option-to-INFO resolution.
// Depends on CommonLibF4 BGSSceneActionPlayerDialogue, scene, and TESTopic runtime layouts.
// Runtime scope is XDI DialogueMenu option identity resolution before response text patching.
// Version-specific logic: uses 1.11.191 active XDI dialogue-map topic ordering only.
// Source-free policy: resolves by runtime INFO pointer/form identity only; no visible/source text matching.
#include "PCH.h"

#include "111191/RuntimeXdiDialogueInfoResolver.h"

#include "RE/B/BGSScene.h"
#include "RE/B/BGSSceneAction.h"
#include "RE/B/BGSSceneActionPlayerDialogue.h"
#include "RE/P/PlayerCharacter.h"
#include "RE/S/SCENE_ACTION_TYPE.h"
#include "RE/T/TESTopic.h"
#include "RE/T/TESTopicInfo.h"

#include <algorithm>

namespace Runtime111191
{
namespace
{
	[[nodiscard]] bool isActivePlayerDialogueAction(const RE::BGSSceneAction* action, std::uint32_t phase)
	{
		return action &&
			   action->GetActionType() == RE::SCENE_ACTION_TYPE::kPlayerDialogue &&
			   (action->status == RE::BGSSceneAction::Status::kRunning ||
				(action->startPhase <= phase && action->endPhase >= phase));
	}

	struct DialogueState
	{
		RE::BGSSceneActionPlayerDialogue* action{ nullptr };
		RE::BGSScene* scene{ nullptr };
	};

	[[nodiscard]] DialogueState currentDialogueState()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* scene = player ? player->GetCurrentScene() : nullptr;
		if (!scene)
		{
			return {};
		}
		for (auto* action : scene->actions)
		{
			if (isActivePlayerDialogueAction(action, scene->currentActivePhase))
			{
				return { static_cast<RE::BGSSceneActionPlayerDialogue*>(action), scene };
			}
		}
		return {};
	}

	void pushUnique(RuntimeXdiDialogueInfoResolver::InfoList& infos, RE::TESTopicInfo* info)
	{
		if (info && std::ranges::find(infos, info) == infos.end())
		{
			infos.push_back(info);
		}
	}

	void appendTopicInfos(RuntimeXdiDialogueInfoResolver::InfoList& infos, RE::TESTopic* topic)
	{
		if (!topic || !topic->topicInfos)
		{
			return;
		}
		for (std::uint32_t i = 0; i < topic->numTopicInfos; ++i)
		{
			auto* info = topic->topicInfos[i];
			if (info && (info->responses.head || info->dataInfo))
			{
				pushUnique(infos, info);
			}
		}
	}
}

namespace RuntimeXdiDialogueInfoResolver
{
	InfoList CollectActiveInfos()
	{
		InfoList infos;
		const auto state = currentDialogueState();
		if (!state.action)
		{
			return infos;
		}
		infos.reserve(16);
		for (auto* topic : state.action->pNPCResponseTopics)
		{
			appendTopicInfos(infos, topic);
		}
		return infos;
	}

	RE::TESTopicInfo* ResolveActiveInfo(const InfoList& infos, std::uint32_t optionID)
	{
		return optionID < infos.size() ? infos[optionID] : nullptr;
	}
}

} // namespace Runtime111191
