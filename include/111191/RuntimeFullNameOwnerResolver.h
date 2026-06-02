// AI CONTEXT: Resolves TESFullName component owners for the 1.11.191 FULL load hook.
// Depends on CommonLibF4 form layouts and Windows page metadata for cached readability checks.
// Runtime scope is Fallout 4 1.11.191 only; offsets are verified for this runtime's loaded form layouts.
// Version-specific logic: owns TESFullName component offsets for WEAP/ARMO/MISC/OMOD/NPC_ in 1.11.191.
// Source-free policy: resolves form identity only; never reads or matches original FULL text.
#pragma once

#include "RE/Fallout.h"

#include <Windows.h>

#ifdef ERROR
#	undef ERROR
#endif

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace Runtime111191::RuntimeFullNameOwnerResolver
{
	struct CacheStats
	{
		std::uint64_t pageHits{ 0 };
		std::uint64_t pageMisses{ 0 };
	};

	namespace detail
	{
		inline std::mutex g_pageLock;
		inline std::unordered_map<std::uintptr_t, bool> g_readablePages;
		inline std::atomic_uint64_t g_pageHits{ 0 };
		inline std::atomic_uint64_t g_pageMisses{ 0 };
		inline thread_local std::uintptr_t g_lastPage{ static_cast<std::uintptr_t>(-1) };
		inline thread_local bool g_lastPageReadable{ false };

		[[nodiscard]] inline std::uintptr_t pageKey(std::uintptr_t address) noexcept
		{
			return address & ~static_cast<std::uintptr_t>(0xFFF);
		}

		[[nodiscard]] inline bool isReadableProtection(DWORD protect) noexcept
		{
			const auto baseProtect = protect & 0xFF;
			return baseProtect == PAGE_READONLY ||
				baseProtect == PAGE_READWRITE ||
				baseProtect == PAGE_WRITECOPY ||
				baseProtect == PAGE_EXECUTE_READ ||
				baseProtect == PAGE_EXECUTE_READWRITE ||
				baseProtect == PAGE_EXECUTE_WRITECOPY;
		}

		[[nodiscard]] inline bool queryReadablePage(std::uintptr_t page) noexcept
		{
			MEMORY_BASIC_INFORMATION info{};
			if (::VirtualQuery(reinterpret_cast<const void*>(page), std::addressof(info), sizeof(info)) != sizeof(info))
			{
				return false;
			}
			return info.State == MEM_COMMIT &&
				(info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0 &&
				isReadableProtection(info.Protect);
		}

		[[nodiscard]] inline bool cachedReadablePage(std::uintptr_t page)
		{
			if (g_lastPage == page)
			{
				g_pageHits.fetch_add(1, std::memory_order_relaxed);
				return g_lastPageReadable;
			}

			{
				std::scoped_lock lock{ g_pageLock };
				if (const auto found = g_readablePages.find(page); found != g_readablePages.end())
				{
					g_pageHits.fetch_add(1, std::memory_order_relaxed);
					g_lastPage = page;
					g_lastPageReadable = found->second;
					return found->second;
				}
			}

			const auto readable = queryReadablePage(page);
			{
				std::scoped_lock lock{ g_pageLock };
				g_readablePages.insert_or_assign(page, readable);
			}
			g_pageMisses.fetch_add(1, std::memory_order_relaxed);
			g_lastPage = page;
			g_lastPageReadable = readable;
			return readable;
		}

		[[nodiscard]] inline bool expectedOffset(RE::ENUM_FORM_ID type, std::size_t offset) noexcept
		{
			switch (offset)
			{
			case 0x20:
				return type == RE::ENUM_FORM_ID::kOMOD;
			case 0x68:
				return type == RE::ENUM_FORM_ID::kWEAP ||
					type == RE::ENUM_FORM_ID::kARMO ||
					type == RE::ENUM_FORM_ID::kMISC;
			case 0x120:
				return type == RE::ENUM_FORM_ID::kNPC_;
			default:
				return false;
			}
		}
	}

	[[nodiscard]] inline CacheStats SnapshotStats() noexcept
	{
		return CacheStats{
			.pageHits = detail::g_pageHits.load(std::memory_order_relaxed),
			.pageMisses = detail::g_pageMisses.load(std::memory_order_relaxed)
		};
	}

	[[nodiscard]] inline bool IsReadable(const void* address, std::size_t size)
	{
		if (!address || size == 0)
		{
			return false;
		}

		const auto begin = reinterpret_cast<std::uintptr_t>(address);
		const auto end = begin + size - 1;
		if (end < begin)
		{
			return false;
		}

		for (auto page = detail::pageKey(begin); page <= detail::pageKey(end); page += 0x1000)
		{
			if (!detail::cachedReadablePage(page))
			{
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] inline RE::TESForm* ResolveKnownReadable(RE::TESFullName* fullName)
	{
		constexpr std::array<std::size_t, 3> kTargetOffsets{
			0x68,
			0x20,
			0x120
		};

		const auto address = reinterpret_cast<std::uintptr_t>(fullName);
		for (const auto offset : kTargetOffsets)
		{
			if (address <= offset)
			{
				continue;
			}

			auto* form = reinterpret_cast<RE::TESForm*>(address - offset);
			if (!IsReadable(form, sizeof(RE::TESForm)) || form->formID == 0)
			{
				continue;
			}

			const auto type = form->GetFormType();
			if (!detail::expectedOffset(type, offset))
			{
				continue;
			}

			auto* runtimeForm = RE::TESForm::GetFormByID(form->formID);
			if (runtimeForm != form)
			{
				continue;
			}
			return runtimeForm->As<RE::TESFullName>() == fullName ? runtimeForm : nullptr;
		}
		return nullptr;
	}
}
