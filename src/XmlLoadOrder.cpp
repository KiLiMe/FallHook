// AI CONTEXT: XML load-order sort implementation.
// Depends on XmlLoadOrder declarations and standard sorting.
// Runtime assumptions: version-neutral Fallout 4 addon priority handling.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: ordering only decides overwrite precedence for resolved slots.
#include "XmlLoadOrder.h"

#include <algorithm>
#include <string_view>

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

	Mode ParseMode(std::string_view mode)
	{
		static constexpr std::pair<std::string_view, Mode> kModes[] = {
			{ "filename", Mode::kFilename },
			{ "plugin", Mode::kPlugin },
		};

		for (const auto& [key, val] : kModes)
		{
			if (mode == key)
			{
				return val;
			}
		}

		return Mode::kPlugin;
	}
}
