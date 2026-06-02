// AI CONTEXT: Validates and installs simple near-jump prologue hooks.
// Depends on REL trampoline/write helpers and a local small x64 prologue decoder.
// Runtime assumptions: version-neutral patch helper; active modules provide verified hook sites.
// Version-specific logic: none; concrete modules own target addresses and patch lengths.
// Source-free policy: contains no translation identity or source-text lookup behavior.
#include "PCH.h"

#include "RuntimePrologueHook.h"

#include "REL/ASM.h"
#include "REL/Utility.h"

#include <Windows.h>

#ifdef ERROR
#	undef ERROR
#endif

namespace
{
	constexpr std::size_t kNearJumpSize{ 5 };
	constexpr std::size_t kAbsoluteJumpSize{ sizeof(REL::ASM::JMP14) };

	struct DecodeResult
	{
		std::size_t length{ 0 };
		bool relative{ false };
	};

	[[nodiscard]] bool hasModRM(std::uint8_t op) noexcept
	{
		switch (op)
		{
		case 0x00: case 0x01: case 0x02: case 0x03: case 0x08: case 0x09: case 0x0A: case 0x0B:
		case 0x10: case 0x11: case 0x12: case 0x13: case 0x18: case 0x19: case 0x1A: case 0x1B:
		case 0x20: case 0x21: case 0x22: case 0x23: case 0x28: case 0x29: case 0x2A: case 0x2B:
		case 0x30: case 0x31: case 0x32: case 0x33: case 0x38: case 0x39: case 0x3A: case 0x3B:
		case 0x63: case 0x69: case 0x6B: case 0x80: case 0x81: case 0x83: case 0x84: case 0x85:
		case 0x86: case 0x87: case 0x88: case 0x89: case 0x8A: case 0x8B: case 0x8C: case 0x8D:
		case 0x8E: case 0x8F: case 0xC0: case 0xC1: case 0xC6: case 0xC7: case 0xD0: case 0xD1:
		case 0xD2: case 0xD3: case 0xF6: case 0xF7: case 0xFE: case 0xFF:
			return true;
		default:
			return false;
		}
	}

	[[nodiscard]] bool hasModRM0F(std::uint8_t op) noexcept
	{
		return (op >= 0x10 && op <= 0x7F) || (op >= 0x90 && op <= 0x9F) ||
			(op >= 0xA3 && op <= 0xAF) || (op >= 0xB0 && op <= 0xBF) || op == 0x1F;
	}

	[[nodiscard]] std::optional<std::size_t> parseModRM(
		const std::uint8_t* code,
		std::size_t max,
		std::size_t& index,
		bool& relative,
		std::uint8_t& modRM)
	{
		if (index >= max)
		{
			return std::nullopt;
		}

		modRM = code[index++];
		const auto mod = static_cast<std::uint8_t>((modRM >> 6) & 0x3);
		const auto rm = static_cast<std::uint8_t>(modRM & 0x7);

		bool hasSib = false;
		std::uint8_t base = 0;
		if (mod != 0x3 && rm == 0x4)
		{
			if (index >= max)
			{
				return std::nullopt;
			}
			const auto sib = code[index++];
			hasSib = true;
			base = static_cast<std::uint8_t>(sib & 0x7);
		}

		if (mod == 0x0)
		{
			relative = rm == 0x5 || (hasSib && base == 0x5);
			index += relative ? 4 : 0;
		}
		else if (mod == 0x1)
		{
			index += 1;
		}
		else if (mod == 0x2)
		{
			index += 4;
		}

		return index <= max ? std::optional{ index } : std::nullopt;
	}

	[[nodiscard]] std::optional<DecodeResult> decodeInstruction(const std::uint8_t* code, std::size_t max)
	{
		std::size_t index = 0;
		bool rexW = false;
		for (;;)
		{
			if (index >= max)
			{
				return std::nullopt;
			}
			const auto prefix = code[index];
			if (prefix >= 0x40 && prefix <= 0x4F)
			{
				rexW = (prefix & 0x08) != 0;
				++index;
				continue;
			}
			if (prefix == 0x66 || prefix == 0x67 || prefix == 0xF2 || prefix == 0xF3 ||
				prefix == 0x2E || prefix == 0x36 || prefix == 0x3E || prefix == 0x26 ||
				prefix == 0x64 || prefix == 0x65)
			{
				++index;
				continue;
			}
			break;
		}

		if (index >= max)
		{
			return std::nullopt;
		}

		const auto op = code[index++];
		DecodeResult result{ .length = index };
		if ((op >= 0x50 && op <= 0x5F) || op == 0x90 || op == 0xC3 || op == 0xCC)
		{
			return result;
		}
		if (op == 0x6A || (op >= 0xB0 && op <= 0xB7))
		{
			result.length = index + 1;
			return result.length <= max ? std::optional{ result } : std::nullopt;
		}
		if (op == 0x68)
		{
			result.length = index + 4;
			return result.length <= max ? std::optional{ result } : std::nullopt;
		}
		if (op >= 0xB8 && op <= 0xBF)
		{
			result.length = index + (rexW ? 8 : 4);
			return result.length <= max ? std::optional{ result } : std::nullopt;
		}
		if (op == 0xE8 || op == 0xE9 || op == 0xEB || (op >= 0x70 && op <= 0x7F))
		{
			result.relative = true;
			return result;
		}

		if (op == 0x0F)
		{
			if (index >= max)
			{
				return std::nullopt;
			}
			const auto op2 = code[index++];
			if (op2 >= 0x80 && op2 <= 0x8F)
			{
				result.length = index + 4;
				result.relative = true;
				return result.length <= max ? std::optional{ result } : std::nullopt;
			}
			if (!hasModRM0F(op2))
			{
				return std::nullopt;
			}
		}
		else if (!hasModRM(op))
		{
			return std::nullopt;
		}

		std::uint8_t modRM = 0;
		auto parsed = parseModRM(code, max, index, result.relative, modRM);
		if (!parsed)
		{
			return std::nullopt;
		}

		switch (op)
		{
		case 0x80: case 0x83: case 0xC0: case 0xC1: case 0xC6: case 0x6B:
			index += 1;
			break;
		case 0x69: case 0x81: case 0xC7:
			index += 4;
			break;
		case 0xF6:
			index += (((modRM >> 3) & 0x7) == 0) ? 1 : 0;
			break;
		case 0xF7:
			index += (((modRM >> 3) & 0x7) == 0) ? 4 : 0;
			break;
		default:
			break;
		}

		result.length = index;
		return index <= max ? std::optional{ result } : std::nullopt;
	}

	[[nodiscard]] std::optional<std::size_t> getPatchLength(const std::uint8_t* target, std::size_t minimumLength, std::size_t maxBytes)
	{
		std::size_t patchLength = 0;
		while (patchLength < minimumLength)
		{
			auto decoded = decodeInstruction(target + patchLength, maxBytes - patchLength);
			if (!decoded || decoded->length == 0 || decoded->relative)
			{
				return std::nullopt;
			}
			patchLength += decoded->length;
			if (patchLength > maxBytes)
			{
				return std::nullopt;
			}
		}
		return patchLength;
	}

	[[nodiscard]] bool isShortConditionalBranch(std::uint8_t op) noexcept
	{
		return op >= 0x70 && op <= 0x7F;
	}

	[[nodiscard]] bool isNearConditionalBranch(const std::uint8_t* bytes, std::size_t available) noexcept
	{
		return available >= 6 && bytes[0] == 0x0F && bytes[1] >= 0x80 && bytes[1] <= 0x8F;
	}

	[[nodiscard]] std::int32_t readRel32(const std::uint8_t* bytes) noexcept
	{
		std::int32_t value = 0;
		std::memcpy(std::addressof(value), bytes, sizeof(value));
		return value;
	}

	void writeRel32(std::uint8_t* bytes, std::int32_t value) noexcept
	{
		std::memcpy(bytes, std::addressof(value), sizeof(value));
	}

	[[nodiscard]] std::optional<RuntimePrologueHook::InstallResult> tryInstallLeadingJcc(
		std::uintptr_t target,
		std::uintptr_t detour,
		std::size_t maxPatchBytes,
		std::string prologueBytes)
	{
		auto* targetBytes = reinterpret_cast<std::uint8_t*>(target);
		const auto first = decodeInstruction(targetBytes, maxPatchBytes);
		if (!first || first->length == 0 || first->relative)
		{
			return std::nullopt;
		}

		const auto branchOffset = first->length;
		if (branchOffset + 2 > maxPatchBytes)
		{
			return std::nullopt;
		}

		std::size_t branchLength = 0;
		std::uintptr_t branchTarget = 0;
		const auto branchBytes = targetBytes + branchOffset;
		if (isShortConditionalBranch(branchBytes[0]))
		{
			branchLength = 2;
			branchTarget = static_cast<std::uintptr_t>(
				static_cast<std::intptr_t>(target + branchOffset + branchLength) +
				static_cast<std::int8_t>(branchBytes[1]));
		}
		else if (isNearConditionalBranch(branchBytes, maxPatchBytes - branchOffset))
		{
			branchLength = 6;
			branchTarget = static_cast<std::uintptr_t>(
				static_cast<std::intptr_t>(target + branchOffset + branchLength) +
				readRel32(branchBytes + 2));
		}
		else
		{
			return std::nullopt;
		}

		const auto patchLength = branchOffset + branchLength;
		const auto continueTarget = target + patchLength;

		auto& trampoline = REL::GetTrampoline();
		const auto trampolineSize = first->length + branchLength + (kAbsoluteJumpSize * 2);
		auto* original = static_cast<std::byte*>(trampoline.allocate(trampolineSize));
		auto* originalBytes = reinterpret_cast<std::uint8_t*>(original);

		std::memcpy(originalBytes, targetBytes, first->length);
		if (branchLength == 2)
		{
			originalBytes[first->length] = branchBytes[0];
			originalBytes[first->length + 1] = static_cast<std::uint8_t>(kAbsoluteJumpSize);
		}
		else
		{
			originalBytes[first->length] = branchBytes[0];
			originalBytes[first->length + 1] = branchBytes[1];
			writeRel32(originalBytes + first->length + 2, static_cast<std::int32_t>(kAbsoluteJumpSize));
		}

		const REL::ASM::JMP14 continueJump{ continueTarget };
		const REL::ASM::JMP14 branchJump{ branchTarget };
		std::memcpy(original + first->length + branchLength, std::addressof(continueJump), sizeof(continueJump));
		std::memcpy(original + first->length + branchLength + kAbsoluteJumpSize, std::addressof(branchJump), sizeof(branchJump));

		trampoline.write_jmp<kNearJumpSize>(target, detour);
		if (patchLength > kNearJumpSize)
		{
			REL::WriteSafeFill(target + kNearJumpSize, 0x90, patchLength - kNearJumpSize);
		}

		::FlushInstructionCache(::GetCurrentProcess(), reinterpret_cast<void*>(target), patchLength);
		::FlushInstructionCache(::GetCurrentProcess(), original, trampolineSize);

		return RuntimePrologueHook::InstallResult{
			.installed = true,
			.original = reinterpret_cast<std::uintptr_t>(original),
			.patchLength = patchLength,
			.prologueBytes = std::move(prologueBytes)
		};
	}
}

namespace RuntimePrologueHook
{
	std::string FormatBytes(const std::uint8_t* bytes, std::size_t count)
	{
		std::string out;
		out.reserve(count * 3);
		for (std::size_t i = 0; i < count; ++i)
		{
			if (i != 0)
			{
				out.push_back(' ');
			}
			out += std::format("{:02X}", bytes[i]);
		}
		return out;
	}

	InstallResult InstallJump(std::uintptr_t target, std::uintptr_t detour, std::size_t maxPatchBytes)
	{
		InstallResult result;
		auto* targetBytes = reinterpret_cast<std::uint8_t*>(target);
		result.prologueBytes = FormatBytes(targetBytes, maxPatchBytes);

		const auto patchLength = getPatchLength(targetBytes, kNearJumpSize, maxPatchBytes);
		if (!patchLength)
		{
			if (auto branchy = tryInstallLeadingJcc(target, detour, maxPatchBytes, result.prologueBytes))
			{
				return *branchy;
			}
			return result;
		}

		auto& trampoline = REL::GetTrampoline();
		auto* original = static_cast<std::byte*>(trampoline.allocate(*patchLength + kAbsoluteJumpSize));
		std::memcpy(original, targetBytes, *patchLength);

		const REL::ASM::JMP14 jumpBack{ target + *patchLength };
		std::memcpy(original + *patchLength, std::addressof(jumpBack), sizeof(jumpBack));

		trampoline.write_jmp<kNearJumpSize>(target, detour);
		if (*patchLength > kNearJumpSize)
		{
			REL::WriteSafeFill(target + kNearJumpSize, 0x90, *patchLength - kNearJumpSize);
		}

		::FlushInstructionCache(::GetCurrentProcess(), reinterpret_cast<void*>(target), *patchLength);
		::FlushInstructionCache(::GetCurrentProcess(), original, *patchLength + kAbsoluteJumpSize);

		result.installed = true;
		result.original = reinterpret_cast<std::uintptr_t>(original);
		result.patchLength = *patchLength;
		return result;
	}
}
