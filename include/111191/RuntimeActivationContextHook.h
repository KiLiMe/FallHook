// AI CONTEXT: Installs the TESObjectREFR context hook that captures activation identity for functional lookup.
// Depends on CommonLibF4 TESObjectREFR runtime objects and hook/watch utilities.
// Runtime scope is Fallout 4 1.11.191 reference/base identity capture.
// Version-specific logic: fixed 1.11.191 Address Library ID 2201128 only.
// Source-free policy: captures form and editorID identity; displayed text is diagnostic only.
#pragma once

namespace Runtime111191::RuntimeActivationContextHook
{
	void Install();
}
