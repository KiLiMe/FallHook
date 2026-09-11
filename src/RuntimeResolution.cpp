// AI CONTEXT: Computes stable runtime indexes for translation targets.
// Depends on RuntimeResolution declarations only.
// Runtime assumptions: version-neutral Fallout 4 record semantics.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: FULL/NPC FULL preserve REC ids so indexed fragments stay out of base names.
#include "RuntimeResolution.h"

#include <format>
#include <type_traits>

namespace RuntimeResolution
{
	std::string MakeRuntimeLogKey(std::uint32_t formID, TranslationType type, std::uint32_t index)
	{
		return std::format("{:08X}|{}|{}", formID, static_cast<std::underlying_type_t<TranslationType>>(type), index);
	}

	std::optional<std::uint32_t> GetSemanticIndex(const XmlTranslationEntry& entry, TranslationType type)
	{
		switch (type)
		{
		case TranslationType::kFullName:
		case TranslationType::kNpcFullName:
			return entry.index;
		case TranslationType::kLocationFullName:
			return std::nullopt;
		case TranslationType::kButtonText1:
		case TranslationType::kButtonText2:
		case TranslationType::kBodyPartName:
		case TranslationType::kFactionMaleRank:
		case TranslationType::kFactionFemaleRank:
		case TranslationType::kPerkVerb:
		case TranslationType::kRuntimeLegacy:
		case TranslationType::kQuestObjective:
		case TranslationType::kTerminalResultText:
		case TranslationType::kTerminalBodyText:
		case TranslationType::kTerminalItemText:
		case TranslationType::kTerminalResponseText:
		case TranslationType::kRuntimeIndex:
			return entry.index.value_or(static_cast<std::uint32_t>(entry.recordOrdinal));
		default:
			return entry.index;
		}
	}
}
