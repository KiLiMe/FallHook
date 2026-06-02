// AI CONTEXT: Installs the TESObjectREFR context hook that feeds HUDRollover action replacement.
// Depends on CommonLibF4 TESObjectREFR runtime objects and hook/watch utilities.
// Runtime scope is Fallout 4 1.10.163 reference/base identity capture.
// Version-specific logic: fixed 1.10.163 Address Library ID only.
// Source-free policy: captures form identity for approved HUD action replacement; displayed text is diagnostic only.
#pragma once

namespace RuntimeActivationContextHook
{
	void Install();
}
