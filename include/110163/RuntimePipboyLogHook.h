// AI CONTEXT: Installs the Pip-Boy log stat visitor hook for GMST-backed log stat labels.
// Depends on the 1.10.163 PipboyLogData::PopulateStatsVisitor vtable and translation maps.
// Runtime scope is Fallout 4 1.10.163 PopulateStatsVisitor only.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: stat keys/editor IDs drive lookup; original label text is never a lookup key.
#pragma once

namespace RuntimePipboyLogHook
{
	void Install();
}
