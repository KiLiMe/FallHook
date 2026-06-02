// AI CONTEXT: Windows-backed UTF-8 NFC normalization implementation.
// Depends on TextNormalization declarations and Win32 string conversion APIs.
// Runtime scope is independent of game version.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: this does not create original-text fallback keys.
#include "TextNormalization.h"

#include <Windows.h>

namespace
{
	std::wstring utf8ToWide(std::string_view input)
	{
		if (input.empty())
		{
			return {};
		}

		const auto size = MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);
		if (size <= 0)
		{
			return {};
		}

		std::wstring result(static_cast<std::size_t>(size), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), result.data(), size);
		return result;
	}

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
}

namespace TextNormalization
{
	std::string NormalizeUtf8(std::string_view text)
	{
		if (text.empty())
		{
			return {};
		}

		const auto wide = utf8ToWide(text);
		if (wide.empty())
		{
			return std::string{ text };
		}

		const auto normalizedSize = NormalizeString(NormalizationC, wide.data(), static_cast<int>(wide.size()), nullptr, 0);
		if (normalizedSize <= 0)
		{
			return std::string{ text };
		}

		std::wstring normalized(static_cast<std::size_t>(normalizedSize), L'\0');
		const auto written = NormalizeString(
			NormalizationC,
			wide.data(),
			static_cast<int>(wide.size()),
			normalized.data(),
			normalizedSize);
		if (written <= 0)
		{
			return std::string{ text };
		}

		normalized.resize(static_cast<std::size_t>(written));
		return wideToUtf8(normalized);
	}
}
