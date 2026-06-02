// AI CONTEXT: Provides guarded reads and writes for selected runtime modules.
// Depends on Windows memory-query implementation in the plugin target.
// Runtime assumptions: version-neutral pointer validation; callers own runtime-specific layout checks.
// Version-specific logic: none; concrete hook modules provide verified offsets and target types.
// Source-free policy: only validates memory and copies already-resolved values; no text lookup happens here.
#pragma once

#include <cstddef>
#include <cstring>
#include <optional>

namespace RuntimeMemorySafety
{
	[[nodiscard]] bool IsReadableMemory(const void* address, std::size_t size) noexcept;
	[[nodiscard]] bool IsWritableMemory(void* address, std::size_t size) noexcept;

	template <class T>
	[[nodiscard]] std::optional<T> ReadUnaligned(const void* base, std::size_t offset) noexcept
	{
		if (!base)
		{
			return std::nullopt;
		}

		const auto* source = reinterpret_cast<const std::byte*>(base) + offset;
		if (!IsReadableMemory(source, sizeof(T)))
		{
			return std::nullopt;
		}

		T value{};
		std::memcpy(std::addressof(value), source, sizeof(T));
		return value;
	}

	template <class T>
	[[nodiscard]] bool WriteUnaligned(void* base, std::size_t offset, const T& value) noexcept
	{
		if (!base)
		{
			return false;
		}

		auto* target = reinterpret_cast<std::byte*>(base) + offset;
		if (!IsWritableMemory(target, sizeof(T)))
		{
			return false;
		}

		std::memcpy(target, std::addressof(value), sizeof(T));
		return true;
	}
}
