// AI CONTEXT: Installs the TESFullName load-time hook for source-free FULL mutation.
// Depends on CommonLibF4/F4SE hook infrastructure through the plugin target.
// Runtime scope is Fallout 4 1.10.163 Address Library ID 180349 only.
// Version-specific logic: this file owns the 1.10.163 hook site.
// Source-free policy: hook applies prebuilt FormID/editorID translations, never Source text.
#pragma once

namespace RuntimeFullNameLoadHook
{
	void Install();
}
