// AI CONTEXT: Exposes patch helpers for the InGameText Scaleform hook entry points.
// Depends on CommonLibF4 REL IDs and runtime trampoline patch support in the .cpp.
// Runtime scope is Fallout 4 1.10.163 InGameText hook installation only.
// Version-specific logic: callers provide verified 1.10.163 Address Library IDs.
// Source-free policy: patching infrastructure only; never performs text lookup.
#pragma once

#include "REL/ID.h"

#include <cstdint>
#include <string_view>

namespace RuntimeInGameTextHookPatch
{
	[[nodiscard]] bool InstallPrologue(REL::ID id, std::uintptr_t detour, std::uintptr_t& original, std::string_view name);
	[[nodiscard]] bool InstallAddRcx8JumpStub(REL::ID id, std::uintptr_t detour, std::uintptr_t& original);
}
