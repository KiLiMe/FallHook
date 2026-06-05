// AI CONTEXT: Collects loaded dialogue topic pointers for source-free DIAL:FULL data mutation.
// Depends on CommonLibF4 TESTopic forms and the global TESForm registry.
// Runtime scope is Fallout 4 1.10.163 loaded dialogue topic enumeration only.
// Version-specific logic: none beyond CommonLibF4 Fallout 4 layout assumptions.
// Source-free policy: only enumerates forms and pointers; no Source text lookup or visible-text matching.
#pragma once

#include <cstddef>
#include <vector>

namespace RE
{
	class TESTopic;
}

namespace RuntimeDialogueForms
{
	struct LoadedForms
	{
		std::vector<RE::TESTopic*> topics;
		std::size_t scannedForms{ 0 };
	};

	[[nodiscard]] LoadedForms Collect();
}
