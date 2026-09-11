// AI CONTEXT: Source-free translation key normalization and serialization.
// Depends on SourceFreeTranslationKey declarations and standard formatting.
// Runtime assumptions: version-neutral Fallout 4 identity material.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: MakeKey has no parameter for original source strings.
#include "SourceFreeTranslationKey.h"

#include <algorithm>
#include <cctype>
#include <format>

namespace
{
	std::string trim(std::string_view value)
	{
		const auto begin = value.find_first_not_of(" \t\r\n");
		if (begin == std::string_view::npos)
		{
			return {};
		}

		const auto end = value.find_last_not_of(" \t\r\n");
		return std::string{ value.substr(begin, end - begin + 1) };
	}

	std::string lower(std::string value)
	{
		std::ranges::transform(value, value.begin(), [](unsigned char ch) {
			return static_cast<char>(std::tolower(ch));
		});
		return value;
	}
}

namespace SourceFreeTranslationKeys
{
	std::string NormalizePluginName(std::string_view pluginName)
	{
		return lower(trim(pluginName));
	}

	std::string NormalizeEditorID(std::string_view editorID)
	{
		return lower(trim(editorID));
	}

	std::string MakeKey(const SourceFreeTranslationKey& key)
	{
		// When a formID is present the plugin name is not part of the
		// identity — the same FormID identifies the same game data
		// regardless of which XML assigned it.  Using a wildcard placeholder
		// lets entries from different Addon values (including "*") collapse
		// into a single catalog record.
		const std::string plugin = key.formID.has_value() ?
			std::string{ "*" } :
			NormalizePluginName(key.pluginName);
		const auto editor = key.editorID ? NormalizeEditorID(*key.editorID) : std::string{};
		return std::format(
			"p={}|f={:08X}|e={}|t={}|i={}|sid={}",
			plugin,
			key.formID.value_or(0),
			editor,
			static_cast<std::uint8_t>(key.type),
			key.index.value_or(0xFFFFFFFFu),
			key.stringID.value_or(0xFFFFFFFFu));
	}
}
