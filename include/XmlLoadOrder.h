// AI CONTEXT: Sorts XML files by plugin priority or filename mode.
// Depends only on standard containers and spans.
// Runtime assumptions: version-neutral Fallout 4 plugin load-order interpretation.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: sorting uses addon/file identity, never source text.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace XmlLoadOrder
{
	enum class Mode
	{
		kPlugin,
		kFilename,
		kLayer
	};

	struct SortEntry
	{
		std::size_t index{ 0 };
		std::string file;
		std::string addon;
		std::uint32_t pluginPriority{ 0 };
		std::uint32_t layerPriority{ 0 };
		bool isOverlay{ false };
	};

	std::vector<std::size_t> SortIndices(std::span<const SortEntry> entries, Mode mode);

	// Numeric filename prefix used by Mode::kLayer, e.g. "010_Fallout4.xml" -> 10.
	// Files without a numeric prefix get the default priority so they stay in the middle.
	[[nodiscard]] std::uint32_t LayerPriority(std::string_view filename);
}
