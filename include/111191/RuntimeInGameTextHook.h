// AI CONTEXT: Installs the approved global in-game TXT source-key UI hook.
// Depends on RuntimeInGameTextSettings; implementation owns verified 1.11.191 Scaleform targets.
// Runtime scope is Fallout 4 1.11.191 Scaleform translator AddTranslations/AddTranslation plus Translate.
// Version-specific logic: uses verified 1.11.191 IDs; SetText is recorded but intentionally inactive.
// Source-free policy: this is the isolated [InGameTextHook] TXT exception; XML lookup is untouched.
#pragma once

#include "RuntimeInGameTextSettings.h"

namespace Runtime111191::RuntimeInGameTextHook
{
	void Install(const RuntimeInGameTextSettings::Values& settings);
}
