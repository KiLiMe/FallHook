// AI CONTEXT: Installs HUDRollover hooks for approved action replacement and diagnostics.
// Depends on debug settings, activation context, action patch support, and prologue hooks.
// Runtime scope is the user-approved Fallout 4 1.11.191 HUDRollover post-data action exception.
// Version-specific logic: installs verified 1.11.191 moved HUDRollover REL IDs 2221994/2221996/2221997/2222000/2222029.
// Source-free policy: replacements use form/sID/editor identity; visible text is diagnostic only.
#pragma once

namespace Runtime111191::RuntimeHudRolloverHook
{
	void Install();
}
