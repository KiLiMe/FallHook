// AI CONTEXT: Loads the independent [Watchdog] switch from FallHook.ini.
// Depends on RuntimeWatchdogSettings and Win32 module-path discovery.
// Runtime assumptions: version-neutral diagnostics setting read before active module load.
// Version-specific logic: none; this file must not read runtime compatibility settings.
// Source-free policy: reads diagnostics configuration only; never alters translation identity.
#include "PCH.h"

#include "RuntimeWatchdogSettings.h"

#include <Windows.h>

#ifdef ERROR
#	undef ERROR
#endif

#include <cctype>
#include <fstream>
#include <limits>

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

	std::uint32_t parseU32(std::string_view value, std::uint32_t fallback)
	{
		const auto normalized = trim(value);
		if (normalized.empty())
		{
			return fallback;
		}

		std::uint64_t result = 0;
		for (const auto ch : normalized)
		{
			if (!std::isdigit(static_cast<unsigned char>(ch)))
			{
				return fallback;
			}
			result = (result * 10u) + static_cast<std::uint32_t>(ch - '0');
			if (result > std::numeric_limits<std::uint32_t>::max())
			{
				return fallback;
			}
		}
		return static_cast<std::uint32_t>(result);
	}
}

namespace RuntimeWatchdogSettings
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
			if (section == "watchdog" && key == "enable")
			{
				settings.enable = parseBool(value, settings.enable);
			}
			else if (section == "activitywatchdog")
			{
				if (key == "enable")
				{
					settings.activity.enable = parseBool(value, settings.activity.enable);
				}
				else if (key == "logbegin")
				{
					settings.activity.logBegin = parseBool(value, settings.activity.logBegin);
				}
				else if (key == "logend")
				{
					settings.activity.logEnd = parseBool(value, settings.activity.logEnd);
				}
				else if (key == "logfastcalls")
				{
					settings.activity.logFastCalls = parseBool(value, settings.activity.logFastCalls);
				}
				else if (key == "slowus")
				{
					settings.activity.slowUs = parseU32(value, settings.activity.slowUs);
				}
				else if (key == "burstwindowms")
				{
					settings.activity.burstWindowMs = parseU32(value, settings.activity.burstWindowMs);
				}
				else if (key == "burstcallwarn")
				{
					settings.activity.burstCallWarn = parseU32(value, settings.activity.burstCallWarn);
				}
				else if (key == "includeworkscopes")
				{
					settings.activity.includeWorkScopes = parseBool(value, settings.activity.includeWorkScopes);
				}
				else if (key == "includethreaddepth")
				{
					settings.activity.includeThreadDepth = parseBool(value, settings.activity.includeThreadDepth);
				}
			}
		}

		return settings;
	}
}
