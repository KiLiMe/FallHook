// AI CONTEXT: Resolves source-free form identities against the live Fallout 4 load order.
// Depends on CommonLibF4 runtime data and SourceFreeTranslationData editor/form identities.
// Runtime scope is Fallout 4 1.11.191 form resolution only.
// Version-specific logic: Fallout 4 1.11.191 only; no alternate runtime branches.
// Source-free policy: resolves by plugin/FormID or editor ID; never matches original text.
#pragma once

#include "Shared.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace RE
{
	class TESForm;
}

namespace Runtime111191::RuntimeFormResolver
{
	[[nodiscard]] std::optional<std::uint32_t> ResolveRawFormID(std::uint32_t rawFormID, std::string_view pluginName);
	[[nodiscard]] RE::TESForm* ResolveRawForm(std::uint32_t rawFormID, std::string_view pluginName);
	[[nodiscard]] RE::TESForm* ResolveEditorForm(const SourceFreeTranslationData& data);
	void ClearLiveFormCache();
}
