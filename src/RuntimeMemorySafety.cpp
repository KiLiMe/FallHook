// AI CONTEXT: Implements guarded runtime memory access checks for hook reads and writes.
// Depends on Windows VirtualQuery and RuntimeMemorySafety declarations.
// Runtime assumptions: version-neutral pointer validation for selected runtime modules.
// Version-specific logic: none; callers own runtime-specific layout and offset checks.
// Source-free policy: this module reads no text and performs no translation lookup.
#include "PCH.h"

#include "RuntimeMemorySafety.h"

#include <Windows.h>

#ifdef ERROR
#	undef ERROR
#endif

namespace RuntimeMemorySafety
{
	namespace
	{
		[[nodiscard]] bool isReadableProtection(DWORD protect) noexcept
		{
			const auto baseProtect = protect & 0xFF;
			return baseProtect == PAGE_READONLY ||
				baseProtect == PAGE_READWRITE ||
				baseProtect == PAGE_WRITECOPY ||
				baseProtect == PAGE_EXECUTE_READ ||
				baseProtect == PAGE_EXECUTE_READWRITE ||
				baseProtect == PAGE_EXECUTE_WRITECOPY;
		}

		[[nodiscard]] bool isWritableProtection(DWORD protect) noexcept
		{
			const auto baseProtect = protect & 0xFF;
			return baseProtect == PAGE_READWRITE ||
				baseProtect == PAGE_WRITECOPY ||
				baseProtect == PAGE_EXECUTE_READWRITE ||
				baseProtect == PAGE_EXECUTE_WRITECOPY;
		}

		template <class ProtectionPredicate>
		[[nodiscard]] bool hasCommittedMemory(const void* address, std::size_t size, ProtectionPredicate protection) noexcept
		{
			if (!address || size == 0)
			{
				return false;
			}

			const auto begin = reinterpret_cast<std::uintptr_t>(address);
			const auto end = begin + size;
			if (end < begin)
			{
				return false;
			}

			for (auto cursor = begin; cursor < end;)
			{
				MEMORY_BASIC_INFORMATION info{};
				if (::VirtualQuery(reinterpret_cast<const void*>(cursor), std::addressof(info), sizeof(info)) != sizeof(info))
				{
					return false;
				}
				if (info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
				{
					return false;
				}
				if (!protection(info.Protect))
				{
					return false;
				}

				const auto regionEnd = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
				if (regionEnd <= cursor)
				{
					return false;
				}
				cursor = regionEnd;
			}

			return true;
		}
	}

	bool IsReadableMemory(const void* address, std::size_t size) noexcept
	{
		return hasCommittedMemory(address, size, isReadableProtection);
	}

	bool IsWritableMemory(void* address, std::size_t size) noexcept
	{
		return hasCommittedMemory(address, size, isWritableProtection);
	}
}
