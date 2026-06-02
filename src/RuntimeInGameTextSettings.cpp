// AI CONTEXT: Loads [InGameTextHook] settings from FallHook.ini.
// Depends on RuntimeInGameTextSettings and Win32 path discovery.
// Runtime assumptions: version-neutral setting consumed only by the selected runtime module.
// Version-specific logic: none; hook offsets and UI sites live in version modules.
// Source-free policy: enables only the approved TXT source-keyed UI hook exception.
#include "PCH.h"

#include "RuntimeInGameTextSettings.h"

#include <Windows.h>

#ifdef ERROR
#	undef ERROR
#endif

#include <cctype>
#include <fstream>

namespace
{
	std::filesystem::path gameDirectory()
	{
		std::wstring buffer(MAX_PATH, L'\0');
		const auto size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
		if (size == 0)
		{
			return {};
		}

		buffer.resize(size);
		return std::filesystem::path{ buffer }.parent_path();
	}

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
		for (auto& ch : value)
		{
			ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
		}
		return value;
	}

	bool parseBool(std::string_view value, bool fallback)
	{
		const auto normalized = lower(trim(value));
		if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on")
		{
			return true;
		}
		if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off")
		{
			return false;
		}
		return fallback;
	}
}

namespace RuntimeInGameTextSettings
{
	Values Load()
	{
		Values settings;
		const auto path = gameDirectory() / "Data" / "F4SE" / "Plugins" / "FallHook.ini";
		std::ifstream input(path);
		if (!input)
		{
			return settings;
		}

		std::string section;
		std::string line;
		while (std::getline(input, line))
		{
			auto text = trim(line.substr(0, line.find_first_of(";#")));
			if (text.empty())
			{
				continue;
			}
			if (text.front() == '[' && text.back() == ']')
			{
				section = lower(trim(std::string_view{ text }.substr(1, text.size() - 2)));
				continue;
			}

			const auto equals = text.find('=');
			if (equals == std::string::npos)
			{
				continue;
			}

			const auto key = lower(trim(std::string_view{ text }.substr(0, equals)));
			const auto value = trim(std::string_view{ text }.substr(equals + 1));
			if (section != "ingametexthook")
			{
				continue;
			}
			if (key == "enable" || key == "enablehook")
			{
				settings.enable = parseBool(value, settings.enable);
			}
			else if (key == "lograw")
			{
				settings.logRaw = parseBool(value, settings.logRaw);
			}
		}

		return settings;
	}
}
