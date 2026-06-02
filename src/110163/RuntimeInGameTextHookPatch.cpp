// AI CONTEXT: Implements patch helpers for the InGameText Scaleform hook entry points.
// Depends on RuntimePrologueHook, RuntimeMemorySafety, and REL trampoline/write helpers.
// Runtime scope is Fallout 4 1.10.163 InGameText hook installation only.
// Version-specific logic: validates the known 1.10.163 AddTranslation RCX+8 jump stub.
// Source-free policy: patching infrastructure only; never performs text lookup.
#include "PCH.h"

#include "110163/RuntimeInGameTextHookPatch.h"

#include "RuntimeMemorySafety.h"
#include "RuntimePrologueHook.h"

#include "REL/ASM.h"
#include "REL/Utility.h"

#include <Windows.h>

#ifdef ERROR
#	undef ERROR
#endif

#include <cstring>

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

	bool InstallAddRcx8JumpStub(REL::ID id, std::uintptr_t detour, std::uintptr_t& original)
	{
		REL::Relocation<std::uintptr_t> target{ id };
		auto* bytes = reinterpret_cast<std::uint8_t*>(target.address());
		if (!RuntimeMemorySafety::IsReadableMemory(bytes, kMaxPatchBytes))
		{
			REX::ERROR("{} skipped BSTranslator::AddTranslation hook; unreadable target {:X}.", Plugin::NAME, target.address());
			return false;
		}
		if (bytes[0] != 0x48 || bytes[1] != 0x83 || bytes[2] != 0xC1 || bytes[3] != 0x08 || bytes[4] != 0xE9)
		{
			REX::ERROR(
				"{} skipped BSTranslator::AddTranslation hook; unsupported stub bytes: {}",
				Plugin::NAME,
				RuntimePrologueHook::FormatBytes(bytes, kMaxPatchBytes));
			return false;
		}

		constexpr std::size_t patchLength = 9;
		std::int32_t relative = 0;
		std::memcpy(std::addressof(relative), bytes + 5, sizeof(relative));
		const auto jumpTarget = target.address() + patchLength + relative;
		auto* gateway = static_cast<std::uint8_t*>(REL::GetTrampoline().allocate(4 + sizeof(REL::ASM::JMP14)));
		std::memcpy(gateway, bytes, 4);
		const REL::ASM::JMP14 jump{ jumpTarget };
		std::memcpy(gateway + 4, std::addressof(jump), sizeof(jump));
		original = reinterpret_cast<std::uintptr_t>(gateway);

		REL::GetTrampoline().write_jmp<kNearJumpSize>(target.address(), detour);
		REL::WriteSafeFill(target.address() + kNearJumpSize, 0x90, patchLength - kNearJumpSize);
		::FlushInstructionCache(::GetCurrentProcess(), reinterpret_cast<void*>(target.address()), patchLength);
		::FlushInstructionCache(::GetCurrentProcess(), gateway, 4 + sizeof(REL::ASM::JMP14));
		REX::INFO("{} installed BSTranslator::AddTranslation jump-stub hook at {:X}.", Plugin::NAME, target.address());
		return true;
	}
}
