// AI CONTEXT: Loads shared runtime apply logging settings from FallHook.ini.
// Depends only on the C++ standard library; implementation uses the plugin game directory.
// Runtime assumptions: version-neutral plugin configuration read before active module load.
// Version-specific logic: none; this file must not add runtime compatibility gates.
// Source-free policy: exposes debug flags only; no translation lookup mode is configured here.
#pragma once

namespace RuntimeApplySettings
{
	struct Values
	{
		bool enableDebugLog{ false };
		bool enableDebugInfo{ false };

		[[nodiscard]] bool TraceEnabled() const noexcept
		{
			return enableDebugLog && enableDebugInfo;
		}
	};

	[[nodiscard]] Values Load();
}
