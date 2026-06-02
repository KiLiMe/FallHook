// AI CONTEXT: Installs validated near-jump prologue hooks for selected runtime modules.
// Depends on CommonLibF4 REL trampoline/write helpers in the plugin target.
// Runtime assumptions: version-neutral patch helper; callers provide verified runtime hook sites.
// Version-specific logic: none; concrete modules own target addresses and patch lengths.
// Source-free policy: hook patching is infrastructure and must not introduce text-key lookup.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace RuntimePrologueHook
{
	struct InstallResult
	{
		bool installed{ false };
		std::uintptr_t original{ 0 };
		std::size_t patchLength{ 0 };
		std::string prologueBytes;
	};

	[[nodiscard]] std::string FormatBytes(const std::uint8_t* bytes, std::size_t count);
	[[nodiscard]] InstallResult InstallJump(std::uintptr_t target, std::uintptr_t detour, std::size_t maxPatchBytes = 16);
}
