// AI CONTEXT: Exposes the user-approved post-data HUDRollover action replacement exception.
// Depends on copied activation look context and runtime action destination maps.
// Runtime scope is Fallout 4 1.10.163 HUDRollover::ShowRollover action parameters only.
// Version-specific logic: uses verified 1.10.163 ShowRollover action slots +0x08/+0x10.
// Source-free policy: chooses replacements by form/sID/REC identity; approval permits no Source matching.
#pragma once

#include "110163/RuntimeActivationLookContext.h"

#include <vector>

namespace RuntimeHudRolloverActionPatch
{
	struct Patch
	{
		const void** field{ nullptr };
		const void* original{ nullptr };
	};

	void ConfigureTrace(bool enabled) noexcept;
	[[nodiscard]] std::vector<Patch> TryApply(
		void* parameters,
		const RuntimeActivationLookContext::Snapshot& context);
	void Restore(const std::vector<Patch>& patches);
}
