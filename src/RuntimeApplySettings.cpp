// AI CONTEXT: Loads shared FallHook runtime apply settings from the plugin INI.
// Depends on RuntimeApplySettings declarations and the process game directory.
// Runtime assumptions: version-neutral configuration read before active module load.
// Version-specific logic: none; this file must not read runtime compatibility settings.
// Source-free policy: reads debug gates only; no source-text lookup option is allowed.
#include "PCH.h"

#include "RuntimeApplySettings.h"

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

	RuntimeApplySettings::Values loadFromDisk()
	{
		RuntimeApplySettings::Values settings;
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
			if (section == "logging" && key == "enabledebuglog")
			{
				settings.enableDebugLog = parseBool(value, settings.enableDebugLog);
			}
			else if (section == "logging" && key == "enabledebuginfo")
			{
				settings.enableDebugInfo = parseBool(value, settings.enableDebugInfo);
			}
		}

		return settings;
	}
}

namespace RuntimeApplySettings
{
	Values Load()
	{
		static const Values settings = loadFromDisk();
		return settings;
	}
}
