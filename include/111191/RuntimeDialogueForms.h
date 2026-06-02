// AI CONTEXT: Collects loaded dialogue topic pointers for source-free data mutation.
// Depends on CommonLibF4 TESTopic form types.
// Runtime scope is Fallout 4 1.11.191 loaded dialogue data enumeration only.
// Version-specific logic: none beyond CommonLibF4 Fallout 4 layout assumptions.
// Source-free policy: only enumerates forms and pointers; no Source text lookup or visible-text matching.
#pragma once

#include <cstddef>
#include <vector>

namespace RE
{
	class TESTopic;
}

namespace Runtime111191::RuntimeDialogueForms
{
	struct LoadedForms
	{
		std::vector<RE::TESTopic*> topics;
		std::size_t scannedForms{ 0 };
	};

	[[nodiscard]] LoadedForms Collect();
}
