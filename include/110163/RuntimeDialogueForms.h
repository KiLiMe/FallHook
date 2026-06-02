// AI CONTEXT: Collects loaded dialogue topic and topic-info pointers for source-free data mutation.
// Depends on CommonLibF4 TESTopic/TESTopicInfo form types and guarded runtime memory checks.
// Runtime scope is Fallout 4 1.10.163 loaded dialogue data enumeration only.
// Version-specific logic: none beyond CommonLibF4 Fallout 4 layout assumptions.
// Source-free policy: only enumerates forms and pointers; no Source text lookup or visible-text matching.
#pragma once

#include <cstddef>
#include <vector>

namespace RE
{
	class TESTopic;
	class TESTopicInfo;
}

namespace RuntimeDialogueForms
{
	struct LoadedForms
	{
		std::vector<RE::TESTopicInfo*> infos;
		std::vector<RE::TESTopic*> topics;
		std::size_t scannedForms{ 0 };
		std::size_t duplicateInfos{ 0 };
	};

	[[nodiscard]] LoadedForms Collect();
}
