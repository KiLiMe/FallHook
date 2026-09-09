// AI CONTEXT: Global stringID-only overlay map implementation.
// Provides a single point of truth for ESP-independent translation overrides.
// Every runtime lookup module checks this map first before falling through to
// formID/editorID-based plugin-bound lookups.
#include "RuntimeStringOverlay.h"

#include <mutex>

namespace
{
	std::mutex g_lock;
	RuntimeStringOverlay::OverlayMap g_map;
}

namespace RuntimeStringOverlay
{
	void SetMap(OverlayMap map)
	{
		std::scoped_lock lock{ g_lock };
		g_map = std::move(map);
	}

	const std::string* Lookup(std::uint32_t stringID) noexcept
	{
		std::scoped_lock lock{ g_lock };
		const auto it = g_map.find(stringID);
		return it != g_map.end() ? std::addressof(it->second) : nullptr;
	}

	std::size_t Count() noexcept
	{
		std::scoped_lock lock{ g_lock };
		return g_map.size();
	}
}