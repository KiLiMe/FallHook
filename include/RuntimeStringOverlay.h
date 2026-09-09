// AI CONTEXT: Global sID-only overlay map for ESP-independent translations.
// Overlay XML files live in FallHook/Overlay/ and provide stringID→text mappings
// that are checked by every runtime lookup module before normal plugin-bound lookups.
// Runtime scope: version-neutral; no game APIs or hook addresses.
// Source-free policy: identity is stringID only; no form/editor ID, no plugin binding.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace RuntimeStringOverlay
{
	using OverlayMap = std::unordered_map<std::uint32_t, std::string>;

	/// Replace the global overlay map (called during catalog build).
	void SetMap(OverlayMap map);

	/// Look up a stringID in the overlay map.
	/// Returns nullptr if not found.
	[[nodiscard]] const std::string* Lookup(std::uint32_t stringID) noexcept;

	/// Number of entries in the overlay map.
	[[nodiscard]] std::size_t Count() noexcept;
}