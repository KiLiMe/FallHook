// AI CONTEXT: Installs TESDescription::GetDescription hook for source-free DESC replacement.
// Depends on MessageIconFormatter, RuntimeDescriptionTranslations, and validated RuntimePrologueHook patching.
// Runtime scope is Fallout 4 1.11.240 Address Library ID 2193019.
// Version-specific logic: Fallout 4 1.11.240 only; no alternate runtime branches.
// Source-free policy: hook resolves replacement by form/description identity; original output only donates icon markup.
// Exception note: MESG/DESC icon preservation is approved and must not be treated as source-exact lookup.
#pragma once

namespace Runtime111240::RuntimeDescriptionHook
{
	void Install();
}
