// AI CONTEXT: Installs the TESFullName load-time hook for source-free FULL mutation.
// Depends on CommonLibF4/F4SE hook infrastructure through the plugin target.
// Runtime scope is Fallout 4 1.11.191 Address Library ID 2193215 only.
// Version-specific logic: the hook target is IDA-matched against the 1.10.163 LoadFullNameChunk body.
// Source-free policy: hook applies prebuilt FormID/editorID translations, never Source text.
#pragma once

namespace Runtime111191::RuntimeFullNameLoadHook
{
	void Install();
}
