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
		kFilename
	};

	struct SortEntry
	{
		std::size_t index{ 0 };
		std::string file;
		std::string addon;
		std::uint32_t pluginPriority{ 0 };
	};

	std::vector<std::size_t> SortIndices(std::span<const SortEntry> entries, Mode mode);
}
