// AI CONTEXT: Canonical source-free key builder for translation maps.
// Depends on Shared.h and standard string formatting.
// Runtime assumptions: version-neutral Fallout 4 form and runtime slot identity.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: keys contain plugin/form/editor/string/slot fields only.
#pragma once

#include "Shared.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

struct SourceFreeTranslationKey
{
	std::string pluginName;
	std::optional<std::uint32_t> formID;
	std::optional<std::string> editorID;
	TranslationType type{ TranslationType::kUnknown };
	std::optional<std::uint32_t> index;
	std::optional<std::uint32_t> stringID;
};

namespace SourceFreeTranslationKeys
{
	std::string NormalizePluginName(std::string_view pluginName);
	std::string NormalizeEditorID(std::string_view editorID);
	std::string MakeKey(const SourceFreeTranslationKey& key);
}
