// AI CONTEXT: Exposes the Fallout 4 1.11.240 runtime module to the dispatcher.
// Depends on the version-neutral RuntimeVersionDispatcher module contract.
// Runtime assumptions: owns Fallout 4 runtime 1.11.240 only.
// Version-specific logic: yes; implementation installs only 1.11.240 hooks and data mutation paths.
// Source-free policy: module preserves existing source-free XML identity and approved TXT exception boundaries.
#pragma once

#include "RuntimeVersionDispatcher.h"

namespace Runtime111240::RuntimeModule111240
{
	[[nodiscard]] const RuntimeVersionDispatcher::Module& GetModule() noexcept;
}
