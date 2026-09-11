// AI CONTEXT: Global debug-test override flag for const-apply text mutation.
// When enabled, all text written through RuntimeTextStringAssign is replaced with "樊".
// Controlled by [Debug] TestAll=1 in FallHook.ini.
#pragma once

#include <atomic>

namespace RuntimeDebugTest
{
	inline std::atomic<bool> g_testAll{ false };
}