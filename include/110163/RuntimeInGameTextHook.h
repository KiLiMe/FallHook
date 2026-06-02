// AI CONTEXT: Installs the approved global in-game TXT source-key UI hook.
// Depends on RuntimeInGameTextSettings; implementation owns 1.10.163 Scaleform offsets.
// Runtime scope is Fallout 4 1.10.163 Scaleform/UI text only.
// Version-specific logic: fixed Fallout 4 1.10.163 Address Library IDs in the .cpp.
// Source-free policy: this is the isolated [InGameTextHook] TXT exception; XML lookup is untouched.
#pragma once

#include "RuntimeInGameTextSettings.h"

namespace RuntimeInGameTextHook
{
	void Install(const RuntimeInGameTextSettings::Values& settings);
}
