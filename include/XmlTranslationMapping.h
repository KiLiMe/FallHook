// AI CONTEXT: Maps xTranslator REC signatures to internal Fallout 4 translation types.
// Depends on Shared.h and standard string helpers.
// Runtime assumptions: version-neutral Fallout 4 record categories.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: mapping uses record/field identity, never original text.
#pragma once

#include "Shared.h"

#include <optional>
#include <string>
#include <string_view>

namespace XmlTranslationMapping
{
	std::string NormalizeSignature(std::string_view rec);
	TranslationType GetTranslationType(std::string_view formType);
	std::optional<std::uint32_t> ParseBracketFormID(std::string_view edid);
}
