// AI CONTEXT: Implements the shared 1.11.191 dialogue choice context cache.
// Depends on RuntimeDialogueChoiceTranslations and CommonLibF4 TESTopic/TESTopicInfo layout.
// Runtime scope is Fallout 4 1.11.191 dialogue hooks that need source-free INFO/DIAL identity.
// Version-specific logic: follows TESTopicInfo::dataInfo chains captured by the 1.11.191 dialogue hook.
// Source-free policy: caches form IDs and runtime pointers only; never records visible/source text.
#include "PCH.h"

#include "111191/RuntimeDialogueChoiceContext.h"

#include "RE/T/TESQuest.h"
#include "RE/T/TESTopic.h"
#include "RE/T/TESTopicInfo.h"

#include <algorithm>
#include <chrono>
#include <mutex>

namespace Runtime111191
{
namespace
{
	constexpr auto kContextTtl = std::chrono::milliseconds{ 1500 };

	std::mutex g_contextLock;
	std::array<
		std::array<RuntimeDialogueChoiceContext::CapturedContext, RuntimeDialogueChoiceContext::kContextsPerButton>,
		RuntimeDialogueChoiceContext::kButtonCount>
		g_contexts;
	std::chrono::steady_clock::time_point g_lastContextUpdate{};

	[[nodiscard]] bool isResponseType(std::uint32_t responseType) noexcept
	{
		return responseType < RuntimeDialogueChoiceContext::kButtonCount;
	}

	void pushInfo(RuntimeDialogueChoiceContext::CapturedContext& context, RE::TESTopicInfo* info)
	{
		if (!info || info->formID == 0)
		{
			return;
		}
		for (std::size_t i = 0; i < context.infoCount; ++i)
		{
			if (context.infos[i] == info)
			{
				return;
			}
		}
		if (context.infoCount < context.infos.size())
		{
			context.infos[context.infoCount++] = info;
		}
		auto& choice = context.choice;
		for (std::size_t i = 0; i < choice.infoCount; ++i)
		{
			if (choice.infoFormIDs[i] == info->formID)
			{
				return;
			}
		}
		if (choice.infoCount < choice.infoFormIDs.size())
		{
			choice.infoFormIDs[choice.infoCount++] = info->formID;
		}
	}
}

namespace RuntimeDialogueChoiceContext
{
	CapturedContext Make(RE::TESTopicInfo* info, std::uint32_t responseType)
	{
		CapturedContext context;
		context.choice.responseType = responseType;
		for (auto* current = info; current && context.infoCount < context.infos.size(); current = current->dataInfo)
		{
			pushInfo(context, current);
		}
		auto* topic = info ? info->parentTopic : nullptr;
		context.choice.topicFormID = topic ? topic->formID : 0;
		context.choice.questFormID = topic && topic->ownerQuest ? topic->ownerQuest->formID : 0;
		return context;
	}

	void Remember(std::uint32_t responseType, const CapturedContext& context)
	{
		if (!isResponseType(responseType) || (context.choice.infoCount == 0 && context.choice.topicFormID == 0))
		{
			return;
		}
		const auto now = std::chrono::steady_clock::now();
		std::scoped_lock lock{ g_contextLock };
		if (g_lastContextUpdate.time_since_epoch().count() == 0 || now - g_lastContextUpdate > kContextTtl)
		{
			g_contexts = {};
		}
		g_lastContextUpdate = now;
		auto& slots = g_contexts[responseType];
		std::move_backward(slots.begin(), slots.end() - 1, slots.end());
		slots.front() = context;
	}

	std::array<CapturedContext, kContextsPerButton> Snapshot(std::uint32_t responseType)
	{
		std::scoped_lock lock{ g_contextLock };
		return isResponseType(responseType) ? g_contexts[responseType] : std::array<CapturedContext, kContextsPerButton>{};
	}
}

} // namespace Runtime111191
