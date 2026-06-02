// AI CONTEXT: Exposes patch helpers for the InGameText Scaleform hook entry points.
// Depends on CommonLibF4 REL IDs and runtime trampoline patch support in the .cpp.
// Runtime scope is Fallout 4 1.11.191 InGameText hook installation only.
// Version-specific logic: callers provide verified 1.11.191 IDs or validated raw stub addresses.
// Source-free policy: patching infrastructure only; never performs text lookup.
#pragma once

#include "REL/ID.h"

#include <cstdint>
#include <string_view>

namespace Runtime111191::RuntimeInGameTextHookPatch
{
	[[nodiscard]] bool InstallPrologue(REL::ID id, std::uintptr_t detour, std::uintptr_t& original, std::string_view name);
	[[nodiscard]] bool InstallAddRcx8LeaR9JumpStub(std::uintptr_t target, std::uintptr_t detour, std::uintptr_t& original);
}
