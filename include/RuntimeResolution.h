// AI CONTEXT: Runtime index helpers for source-free translation slots.
// Depends on Shared.h and parsed XML entries.
// Runtime assumptions: version-neutral Fallout 4 semantic record indexing.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: indexes come from REC attributes or record ordinals, not original text.
#pragma once

#include "Shared.h"
#include "XmlTranslationParser.h"

#include <optional>
#include <string>

namespace RuntimeResolution
{
	std::optional<std::uint32_t> GetSemanticIndex(const XmlTranslationEntry& entry, TranslationType type);
	std::string MakeRuntimeLogKey(std::uint32_t formID, TranslationType type, std::uint32_t index);
}
