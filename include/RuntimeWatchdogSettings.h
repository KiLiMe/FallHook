// AI CONTEXT: Reads independent runtime watchdog settings from FallHook.ini.
// Depends only on standard types; implementation reads the plugin game-directory INI.
// Runtime assumptions: version-neutral setting read before active module hook installation.
// Version-specific logic: none; this file must not add runtime compatibility gates.
// Source-free policy: controls timing diagnostics only; no translation lookup behavior exists here.
#pragma once

#include "RuntimeActivityWatch.h"

namespace RuntimeWatchdogSettings
{
	struct Values
	{
		bool enable{ false };
		RuntimeActivityWatch::Settings activity;
	};

	[[nodiscard]] Values Load();
}
