// AI CONTEXT: Installs HUDRollover hooks for approved action replacement and debug diagnostics.
// Depends on debug settings, activation context, action maps, and prologue hook support.
// Runtime scope is the user-approved Fallout 4 1.10.163 HUDRollover post-data action exception.
// Version-specific logic: uses verified 1.10.163 HUDRollover RVAs/Address Library ID.
// Source-free policy: replacements use source-free identity maps; approval does not permit Source matching.
#pragma once

namespace RuntimeHudRolloverHook
{
	void Install();
}
