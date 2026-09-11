// AI CONTEXT: Implements patch helpers for the InGameText Scaleform hook entry points.
// Depends on RuntimePrologueHook, RuntimeMemorySafety, and CommonLibF4 REL trampoline helpers.
// Runtime scope is Fallout 4 1.11.240 InGameText hook installation only.
// Version-specific logic: installs verified 1.11.240 Address Library prologue hooks and AddTranslation stub.
// Source-free policy: patching infrastructure only; never performs text lookup.
#include "PCH.h"

#include "111240/RuntimeInGameTextHookPatch.h"

#include "Plugin.h"
#include "RuntimeMemorySafety.h"
#include "RuntimePrologueHook.h"

#include "REL/ASM.h"
#include "REL/Utility.h"

#include <Windows.h>

#ifdef ERROR
#	undef ERROR
#endif

#include <cstring>

namespace Runtime111240
{
namespace
{
	constexpr std::size_t kMaxPatchBytes{ 16 };
	constexpr std::size_t kNearJumpSize{ 5 };
}

namespace RuntimeInGameTextHookPatch
{
	bool InstallPrologue(REL::ID id, std::uintptr_t detour, std::uintptr_t& original, std::string_view name)
	{
		REL::Relocation<std::uintptr_t> target{ id };
		const auto result = RuntimePrologueHook::InstallJump(target.address(), detour, kMaxPatchBytes);
		if (!result.installed)
		{
			REX::ERROR("{} skipped {} hook; unsupported prologue bytes: {}", Plugin::NAME, name, result.prologueBytes);
			return false;
		}

		original = result.original;
		REX::INFO("{} installed {} hook at {:X} using Address Library ID {}.", Plugin::NAME, name, target.address(), id.id());
		return true;
	}

	bool InstallAddRcx8LeaR9JumpStub(std::uintptr_t target, std::uintptr_t detour, std::uintptr_t& original)
	{
		auto* bytes = reinterpret_cast<std::uint8_t*>(target);
		if (!RuntimeMemorySafety::IsReadableMemory(bytes, kMaxPatchBytes))
		{
			REX::ERROR("{} skipped BSTranslator::AddTranslation hook; unreadable target {:X}.", Plugin::NAME, target);
			return false;
		}
		if (bytes[0] != 0x48 || bytes[1] != 0x83 || bytes[2] != 0xC1 || bytes[3] != 0x08 ||
			bytes[4] != 0x4C || bytes[5] != 0x8D || bytes[6] != 0x4C || bytes[7] != 0x24 ||
			bytes[8] != 0x08 || bytes[9] != 0xE9)
		{
			REX::ERROR(
				"{} skipped BSTranslator::AddTranslation hook; unsupported 111240 stub bytes: {}",
				Plugin::NAME,
				RuntimePrologueHook::FormatBytes(bytes, kMaxPatchBytes));
			return false;
		}

		constexpr std::size_t prefixLength = 9;
		constexpr std::size_t patchLength = 14;
		std::int32_t relative = 0;
		std::memcpy(std::addressof(relative), bytes + 10, sizeof(relative));
		const auto jumpTarget = target + patchLength + relative;
		auto* gateway = static_cast<std::uint8_t*>(REL::GetTrampoline().allocate(prefixLength + sizeof(REL::ASM::JMP14)));
		std::memcpy(gateway, bytes, prefixLength);
		const REL::ASM::JMP14 jump{ jumpTarget };
		std::memcpy(gateway + prefixLength, std::addressof(jump), sizeof(jump));
		original = reinterpret_cast<std::uintptr_t>(gateway);

		REL::GetTrampoline().write_jmp<kNearJumpSize>(target, detour);
		REL::WriteSafeFill(target + kNearJumpSize, 0x90, patchLength - kNearJumpSize);
		::FlushInstructionCache(::GetCurrentProcess(), reinterpret_cast<void*>(target), patchLength);
		::FlushInstructionCache(::GetCurrentProcess(), gateway, prefixLength + sizeof(REL::ASM::JMP14));
		REX::INFO("{} installed BSTranslator::AddTranslation 111240 jump-stub hook at {:X}.", Plugin::NAME, target);
		return true;
	}
}

} // namespace Runtime111240
