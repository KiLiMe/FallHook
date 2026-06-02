// AI CONTEXT: Implements 1.10.163 XDI option candidate resolution.
// Depends on RuntimeDialogueChoiceContext, RuntimeXdiDialogueOptionTranslations, and TESTopicInfo layout.
// Runtime scope is XDI DialogueMenu option selection before Scaleform text patching.
// Version-specific logic: follows 1.10.163 TESTopicInfo::dataInfo chains.
// Source-free policy: requires INFO:RNAM for captured fallback selection; never uses visible/source text.
#include "PCH.h"

#include "110163/RuntimeXdiDialogueOptionResolver.h"

#include "110163/RuntimeDialogueChoiceContext.h"

#include "RE/T/TESTopicInfo.h"

#include <cstddef>
#include <utility>

namespace
{
	[[nodiscard]] bool hasOptionText(const RuntimeXdiDialogueOptionTranslations::OptionText& text)
	{
		return (text.hasPrompt && !text.prompt.empty()) || (text.hasResponse && !text.response.empty());
	}
}

namespace RuntimeXdiDialogueOptionResolver
{
	bool HasText(const ResolvedOption& resolved)
	{
		return hasOptionText(resolved.text);
	}

	ResolvedOption ResolveInfo(RE::TESTopicInfo* info)
	{
		ResolvedOption fallback;
		for (auto* current = info; current; current = current->dataInfo)
		{
			auto text = RuntimeXdiDialogueOptionTranslations::Resolve(current->formID);
			if (!hasOptionText(text))
			{
				continue;
			}
			ResolvedOption resolved{ .text = std::move(text), .info = current };
			if (resolved.text.hasPrompt && !resolved.text.prompt.empty())
			{
				return resolved;
			}
			if (!fallback.info)
			{
				fallback = std::move(resolved);
			}
		}
		return fallback;
	}

	ResolvedOption ResolveCapturedPrompt(std::uint32_t optionID)
	{
		if (optionID >= RuntimeDialogueChoiceContext::kButtonCount)
		{
			return {};
		}
		ResolvedOption miss;
		const auto contexts = RuntimeDialogueChoiceContext::Snapshot(optionID);
		for (const auto& context : contexts)
		{
			for (std::size_t i = 0; i < context.infoCount; ++i)
			{
				const auto* info = context.infos[i];
				if (info && !miss.candidate0)
				{
					miss.candidate0 = info->formID;
				}
				else if (info && !miss.candidate1 && info->formID != miss.candidate0)
				{
					miss.candidate1 = info->formID;
				}
				auto resolved = ResolveInfo(context.infos[i]);
				if (resolved.text.hasPrompt && !resolved.text.prompt.empty())
				{
					return resolved;
				}
			}
		}
		return miss;
	}
}
