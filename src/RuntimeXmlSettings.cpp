// AI CONTEXT: Loads [XML] settings from FallHook.ini.
// Depends only on the C++ standard library; implementation uses the plugin game directory.
// Runtime assumptions: version-neutral plugin configuration read before catalog build.
// Version-specific logic: none; this file must not add runtime compatibility gates.
// Source-free policy: configures XML file ordering only; no translation text is read here.
#include "PCH.h"

#include "RuntimeXmlSettings.h"

#include <Windows.h>

#ifdef ERROR
#	undef ERROR
#endif

#include <cctype>
#include <fstream>
#include <string>
#include <string_view>

namespace
{
	// Same approach as RuntimeWatchdogSettings: resolve the DLL path, then use
	// its directory, which is <game>/Data/F4SE/Plugins.
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
}

namespace RuntimeXmlSettings
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
			if (section == "xml" && key == "loadordermode")
			{
				const auto normalized = lower(value);
				if (normalized == "plugin")
				{
					settings.loadOrderMode = XmlLoadOrder::Mode::kPlugin;
				}
				else if (normalized == "filename")
				{
					settings.loadOrderMode = XmlLoadOrder::Mode::kFilename;
				}
				else if (normalized == "layer")
				{
					settings.loadOrderMode = XmlLoadOrder::Mode::kLayer;
				}
			}
		}

		return settings;
	}
}
