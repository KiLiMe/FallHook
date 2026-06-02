// AI CONTEXT: Exposes the 1.11.191 HUDRollover action replacement path.
// Depends on copied activation look context plus activation/PERK destination maps.
// Runtime scope is Fallout 4 1.11.191 HUDRollover::ShowRollover action parameters only.
// Version-specific logic: patches verified moved ShowRolloverParameters slots +0x08 and +0x10.
// Source-free policy: chooses replacements by form/sID/editor identity; never by live text.
#pragma once

#include "111191/RuntimeActivationLookContext.h"

#include <optional>
#include <string>
#include <vector>

namespace Runtime111191::RuntimeHudRolloverActionPatch
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
