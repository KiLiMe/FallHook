// AI CONTEXT: XML load-order sort implementation.
// Depends on XmlLoadOrder declarations and standard sorting.
// Runtime assumptions: version-neutral Fallout 4 addon priority handling.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: ordering only decides overwrite precedence for resolved slots.
#include "XmlLoadOrder.h"

#include <algorithm>

namespace
{
	constexpr std::uint32_t kDefaultLayer{ 50 };
}

namespace XmlLoadOrder
{
	std::vector<std::size_t> SortIndices(std::span<const SortEntry> entries, Mode mode)
	{
		std::vector<std::size_t> order;
		order.reserve(entries.size());
		for (std::size_t i = 0; i < entries.size(); ++i)
		{
			order.emplace_back(i);
		}

		std::ranges::sort(order, [&](const auto leftIndex, const auto rightIndex) {
			const auto& left = entries[leftIndex];
			const auto& right = entries[rightIndex];

			if (mode == Mode::kLayer)
			{
				// Overlay files always sort after (higher than) normal files
				if (left.isOverlay != right.isOverlay)
				{
					return left.isOverlay < right.isOverlay;
				}

				// Within the same group, sort by layer priority, then filename.
				// Fall back to index so equal (priority, file) pairs keep a
				// deterministic strict weak ordering.
				if (left.layerPriority != right.layerPriority)
				{
					return left.layerPriority < right.layerPriority;
				}

				if (left.file != right.file)
				{
					return left.file < right.file;
				}

				return left.index < right.index;
			}

			if (mode == Mode::kPlugin)
			{
				if (left.pluginPriority != right.pluginPriority)
				{
					return left.pluginPriority > right.pluginPriority;
				}

				return left.file > right.file;
			}

			if (left.file != right.file)
			{
				return left.file < right.file;
			}

			return left.index < right.index;
		});

		return order;
	}

	std::uint32_t LayerPriority(std::string_view filename)
	{
		std::uint32_t value = 0;
		bool hasDigits = false;
		for (const auto ch : filename)
		{
			if (ch >= '0' && ch <= '9')
			{
				// Guard against overflow on absurdly long numeric prefixes.
				if (value > (0xFFFFFFFFu - static_cast<std::uint32_t>(ch - '0')) / 10)
				{
					return kDefaultLayer;
				}
				value = value * 10 + static_cast<std::uint32_t>(ch - '0');
				hasDigits = true;
			}
			else if (ch == '_' || ch == '-' || ch == ' ')
			{
				if (hasDigits)
				{
					return value;
				}
			}
			else
			{
				return hasDigits ? value : kDefaultLayer;
			}
		}
		return hasDigits ? value : kDefaultLayer;
	}
}
