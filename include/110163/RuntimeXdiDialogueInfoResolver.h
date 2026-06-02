// AI CONTEXT: Resolves XDI dialogue option IDs to 1.10.163 TESTopicInfo identities.
// Depends on CommonLibF4 dialogue scene/topic layouts through the plugin target.
// Runtime scope is Fallout 4 1.10.163 XDI DialogueMenu option resolution only.
// Version-specific logic: mirrors 1.10.163 active XDI dialogue-map topic ordering.
// Source-free policy: returns INFO form identity only; visible text and XML Source are never lookup keys.
#pragma once

#include <cstdint>
#include <vector>

namespace RE
{
	class TESTopicInfo;
}

namespace RuntimeXdiDialogueInfoResolver
{
	using InfoList = std::vector<RE::TESTopicInfo*>;

	[[nodiscard]] InfoList CollectActiveInfos();
	[[nodiscard]] RE::TESTopicInfo* ResolveActiveInfo(const InfoList& infos, std::uint32_t optionID);
}
