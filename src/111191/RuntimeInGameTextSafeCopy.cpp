// AI CONTEXT: Implements bounded safe string copying for InGameText runtime hooks.
// Depends on RuntimeMemorySafety to guard game/UI pointers before reading each character.
// Runtime scope is Fallout 4 1.11.191 UI hook pointer reads only.
// Version-specific logic: none.
// Source-free policy: copies text for logging/translation paths; never decides identity.
#include "PCH.h"

#include "111191/RuntimeInGameTextSafeCopy.h"

#include "RuntimeMemorySafety.h"

namespace Runtime111191
{
namespace RuntimeInGameTextSafeCopy
{
	std::wstring Wide(const wchar_t* text, std::size_t limit)
	{
		std::wstring result;
		if (!RuntimeMemorySafety::IsReadableMemory(text, sizeof(wchar_t)))
		{
			return result;
		}
		for (std::size_t i = 0; i < limit; ++i)
		{
			if (!RuntimeMemorySafety::IsReadableMemory(text + i, sizeof(wchar_t)))
			{
				break;
			}
			const auto ch = text[i];
			if (ch == L'\0')
			{
				break;
			}
			result.push_back(ch);
		}
		return result;
	}
}

} // namespace Runtime111191
