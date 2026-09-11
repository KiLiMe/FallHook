// AI CONTEXT: Dispatch interface for source-free runtime const text application.
// Depends on Shared translation data and CommonLibF4 form types in the plugin target.
// Runtime scope is Fallout 4 1.11.240 direct non-hook text application.
// Version-specific logic: Fallout 4 1.11.240 only, including verified REGN:RDMP map-data mutation.
// Source-free policy: dispatches by source-free type/form/index identity only.
#pragma once

#include "Shared.h"

#include <string_view>

namespace RE
{
	class TESForm;
}

namespace Runtime111240::RuntimeTextApply
{
	[[nodiscard]] bool IsAllowed(TranslationType type) noexcept;
	[[nodiscard]] bool ApplyForm(RE::TESForm* form, const SourceFreeTranslationData& data);
	[[nodiscard]] bool ApplyGameSetting(const SourceFreeTranslationData& data);
	[[nodiscard]] bool ApplyGameSetting(std::string_view editorID, std::string_view text);
}
