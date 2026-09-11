// AI CONTEXT: Installs the Pip-Boy log stat visitor hook for GMST-backed log stat labels.
// Depends on the 1.11.240 PipboyLogData::PopulateStatsVisitor vtable and translation maps.
// Runtime scope is Fallout 4 1.11.240 PopulateStatsVisitor only.
// Version-specific logic: Fallout 4 1.11.240 only; no alternate runtime branches.
// Source-free policy: stat keys/editor IDs drive lookup; original label text is never a lookup key.
#pragma once

namespace Runtime111240::RuntimePipboyLogHook
{
	void Install();
}
