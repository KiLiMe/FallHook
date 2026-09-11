// AI CONTEXT: Caches 1.11.240 literal TXT lookups for BSScaleformTranslator::Translate.
// Depends on RuntimeInGameTextDictionary for normalized source-keyed TXT lookup and revision tracking.
// Runtime scope is Fallout 4 1.11.240 InGameText Translate hook hot path only.
// Version-specific logic: none beyond owning 111240 hook cache state; no offsets or hook sites here.
// Source-free policy: caches only approved TXT source-key exception results; XML lookup remains source-free.
#pragma once

#include "RuntimeInGameTextDictionary.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace Runtime111240::RuntimeInGameTextTranslateCache
{
	namespace detail
	{
		inline std::mutex g_lock;
		inline std::uint64_t g_revision{ 0 };
		inline std::unordered_map<std::wstring, std::wstring> g_hits;
		inline std::unordered_set<std::wstring> g_misses;

		inline void syncRevisionLocked()
		{
			const auto revision = RuntimeInGameTextDictionary::Revision();
			if (g_revision == revision)
			{
				return;
			}
			g_hits.clear();
			g_misses.clear();
			g_revision = revision;
		}
	}

	[[nodiscard]] inline std::optional<std::wstring> LookupLiteral(std::wstring_view raw)
	{
		auto key = RuntimeInGameTextDictionary::NormalizeKey(raw);
		if (key.empty())
		{
			return std::nullopt;
		}

		{
			std::scoped_lock lock{ detail::g_lock };
			detail::syncRevisionLocked();
			if (const auto found = detail::g_hits.find(key); found != detail::g_hits.end())
			{
				return found->second;
			}
			if (detail::g_misses.contains(key))
			{
				return std::nullopt;
			}
		}

		auto translated = RuntimeInGameTextDictionary::Lookup(key);
		{
			std::scoped_lock lock{ detail::g_lock };
			detail::syncRevisionLocked();
			if (translated && !translated->empty())
			{
				detail::g_hits.insert_or_assign(std::move(key), *translated);
				return translated;
			}
			detail::g_misses.insert(std::move(key));
		}
		return std::nullopt;
	}
}
