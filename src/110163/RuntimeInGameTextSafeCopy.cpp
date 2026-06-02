// AI CONTEXT: Implements bounded safe string copying for InGameText runtime hooks.
// Depends on RuntimeMemorySafety to guard game/UI pointers before reading each character.
// Runtime scope is Fallout 4 1.10.163 UI hook pointer reads only.
// Version-specific logic: none.
// Source-free policy: copies text for logging/translation paths; never decides identity.
#include "PCH.h"

#include "110163/RuntimeInGameTextSafeCopy.h"

#include "RuntimeMemorySafety.h"

namespace RuntimeInGameTextSafeCopy
{
	namespace
	{
		template <class CharT, class StringT>
		[[nodiscard]] StringT copyBounded(const CharT* text, std::size_t limit)
		{
			StringT result;
			if (!text || limit == 0)
			{
				return result;
			}

			constexpr std::uintptr_t kPageSize = 0x1000;
			std::size_t index = 0;
			while (index < limit)
			{
				const auto address = reinterpret_cast<std::uintptr_t>(text + index);
				const auto pageEnd = (address + kPageSize) & ~(kPageSize - 1);
				const auto bytesInPage = pageEnd > address ? pageEnd - address : sizeof(CharT);
				const auto charsInPage = std::max<std::size_t>(1, bytesInPage / sizeof(CharT));
				const auto count = std::min(limit - index, charsInPage);
				if (!RuntimeMemorySafety::IsReadableMemory(text + index, count * sizeof(CharT)))
				{
					break;
				}

				for (std::size_t local = 0; local < count && index < limit; ++local, ++index)
				{
					const auto ch = text[index];
					if (ch == CharT{})
					{
						return result;
					}
					result.push_back(ch);
				}
			}
			return result;
		}
	}

	std::wstring Wide(const wchar_t* text, std::size_t limit)
	{
		return copyBounded<wchar_t, std::wstring>(text, limit);
	}

	std::string Utf8(const char* text, std::size_t limit)
	{
		return copyBounded<char, std::string>(text, limit);
	}
}
