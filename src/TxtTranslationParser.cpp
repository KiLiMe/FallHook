// AI CONTEXT: TXT translation dictionary parser implementation.
// Depends on Win32 conversion APIs for UTF-16 input and standard file IO.
// Runtime scope is independent of game version.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: parsed TXT source columns must not become record lookup identity.
#include "TxtTranslationParser.h"

#include <Windows.h>

#include <fstream>
#include <iterator>
#include <string_view>

namespace
{
	enum class TextEncoding
	{
		kUtf8,
		kUtf16LE,
		kUtf16BE
	};

	std::string wideToUtf8(std::wstring_view input)
	{
		if (input.empty())
		{
			return {};
		}

		const auto size = WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0, nullptr, nullptr);
		if (size <= 0)
		{
			return {};
		}

		std::string result(static_cast<std::size_t>(size), '\0');
		WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), result.data(), size, nullptr, nullptr);
		return result;
	}

	TextEncoding detectEncoding(std::string_view bytes, std::size_t& offset)
	{
		offset = 0;
		if (bytes.size() >= 3 &&
			static_cast<unsigned char>(bytes[0]) == 0xEF &&
			static_cast<unsigned char>(bytes[1]) == 0xBB &&
			static_cast<unsigned char>(bytes[2]) == 0xBF)
		{
			offset = 3;
			return TextEncoding::kUtf8;
		}
		if (bytes.size() >= 2 &&
			static_cast<unsigned char>(bytes[0]) == 0xFF &&
			static_cast<unsigned char>(bytes[1]) == 0xFE)
		{
			offset = 2;
			return TextEncoding::kUtf16LE;
		}
		if (bytes.size() >= 2 &&
			static_cast<unsigned char>(bytes[0]) == 0xFE &&
			static_cast<unsigned char>(bytes[1]) == 0xFF)
		{
			offset = 2;
			return TextEncoding::kUtf16BE;
		}
		return TextEncoding::kUtf8;
	}

	std::wstring decodeUtf16(std::string_view bytes, bool bigEndian)
	{
		std::wstring result;
		result.reserve(bytes.size() / 2);

		for (std::size_t i = 0; i + 1 < bytes.size(); i += 2)
		{
			const auto lo = static_cast<unsigned char>(bytes[i]);
			const auto hi = static_cast<unsigned char>(bytes[i + 1]);
			const auto codeUnit = bigEndian ?
				static_cast<std::uint16_t>((lo << 8) | hi) :
				static_cast<std::uint16_t>(lo | (hi << 8));
			result.push_back(static_cast<wchar_t>(codeUnit));
		}

		return result;
	}

	bool isIgnorableLine(std::string_view line)
	{
		const auto firstNonWhitespace = line.find_first_not_of(" \t\r");
		if (firstNonWhitespace == std::string_view::npos)
		{
			return true;
		}

		const auto firstChar = line[firstNonWhitespace];
		return firstChar == '#' || firstChar == ';';
	}

	bool isIgnorableLine(std::wstring_view line)
	{
		const auto firstNonWhitespace = line.find_first_not_of(L" \t\r");
		if (firstNonWhitespace == std::wstring_view::npos)
		{
			return true;
		}

		const auto firstChar = line[firstNonWhitespace];
		return firstChar == L'#' || firstChar == L';';
	}

	void parseUtf8Lines(std::string_view content, TxtParseResult& result)
	{
		std::size_t lineNumber = 0;
		std::size_t start = 0;
		while (start <= content.size())
		{
			const auto end = content.find('\n', start);
			auto line = end == std::string_view::npos ?
				content.substr(start) :
				content.substr(start, end - start);

			if (!line.empty() && line.back() == '\r')
			{
				line.remove_suffix(1);
			}

			++lineNumber;
			if (!isIgnorableLine(line))
			{
				const auto separator = line.find('\t');
				if (separator != std::string_view::npos)
				{
					TxtTranslationEntry entry;
					entry.lineNumber = lineNumber;
					entry.source = std::string{ line.substr(0, separator) };
					entry.dest = std::string{ line.substr(separator + 1) };
					result.file.entries.emplace_back(std::move(entry));
				}
			}

			if (end == std::string_view::npos)
			{
				break;
			}
			start = end + 1;
		}
	}

	void parseUtf16Lines(std::wstring_view content, TxtParseResult& result)
	{
		std::size_t lineNumber = 0;
		std::size_t start = 0;
		while (start <= content.size())
		{
			const auto end = content.find(L'\n', start);
			auto line = end == std::wstring_view::npos ?
				content.substr(start) :
				content.substr(start, end - start);

			if (!line.empty() && line.back() == L'\r')
			{
				line.remove_suffix(1);
			}

			++lineNumber;
			if (!isIgnorableLine(line))
			{
				const auto separator = line.find(L'\t');
				if (separator != std::wstring_view::npos)
				{
					TxtTranslationEntry entry;
					entry.lineNumber = lineNumber;
					entry.source = wideToUtf8(line.substr(0, separator));
					entry.dest = wideToUtf8(line.substr(separator + 1));
					result.file.entries.emplace_back(std::move(entry));
				}
			}

			if (end == std::wstring_view::npos)
			{
				break;
			}
			start = end + 1;
		}
	}
}

namespace TxtTranslationParser
{
	TxtParseResult ParseFile(const std::filesystem::path& path)
	{
		TxtParseResult result;
		result.file.path = path;

		std::ifstream input(path, std::ios::binary);
		if (!input.is_open())
		{
			result.error = "failed to open TXT file";
			return result;
		}

		std::string content{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{} };
		std::size_t offset = 0;
		switch (detectEncoding(content, offset))
		{
		case TextEncoding::kUtf8:
			parseUtf8Lines(std::string_view{ content }.substr(offset), result);
			break;
		case TextEncoding::kUtf16LE:
			parseUtf16Lines(decodeUtf16(std::string_view{ content }.substr(offset), false), result);
			break;
		case TextEncoding::kUtf16BE:
			parseUtf16Lines(decodeUtf16(std::string_view{ content }.substr(offset), true), result);
			break;
		}

		result.success = true;
		return result;
	}
}
