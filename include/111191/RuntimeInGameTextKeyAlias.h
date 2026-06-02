// AI CONTEXT: Caches 1.11.191 UI key-to-literal aliases for global in-game UI text.
// Depends on RuntimeInGameTextDictionary and standard library collections.
// Runtime scope is Fallout 4 1.11.191 in-game UI text translation only.
// Version-specific logic: none.
// Source-free policy: stores key-to-visible alias relationships to resolve localization keys; XML remains source-free.
#pragma once

#include "RuntimeInGameTextDictionary.h"

#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Runtime111191::RuntimeInGameTextKeyAlias
{
	namespace detail
	{
		inline std::mutex g_aliasLock;
		inline std::unordered_map<std::wstring, std::wstring> g_literalToKey;
		inline std::unordered_map<std::wstring, std::wstring> g_keyToLiteral;
	}

	[[nodiscard]] inline bool IsRawKey(std::wstring_view text) noexcept
	{
		text = RuntimeInGameTextDictionary::TrimText(text);
		return !text.empty() && text.front() == L'$';
	}

	inline void RememberKeyAlias(std::wstring_view raw, std::wstring_view visible)
	{
		raw = RuntimeInGameTextDictionary::TrimText(raw);
		visible = RuntimeInGameTextDictionary::TrimText(visible);
		if (!IsRawKey(raw) || visible.empty() || visible == raw)
		{
			return;
		}

		std::scoped_lock lock{ detail::g_aliasLock };
		detail::g_literalToKey.insert_or_assign(std::wstring{ visible }, std::wstring{ raw });
		detail::g_keyToLiteral.insert_or_assign(std::wstring{ raw }, std::wstring{ visible });
	}

	[[nodiscard]] inline std::optional<std::wstring> FindKeyAlias(std::wstring_view literal)
	{
		literal = RuntimeInGameTextDictionary::TrimText(literal);
		if (literal.empty() || IsRawKey(literal))
		{
			return std::nullopt;
		}

		std::scoped_lock lock{ detail::g_aliasLock };
		if (const auto it = detail::g_literalToKey.find(std::wstring{ literal }); it != detail::g_literalToKey.end())
		{
			return it->second;
		}
		return std::nullopt;
	}

	[[nodiscard]] inline std::optional<std::wstring> FindVisibleForKey(std::wstring_view raw)
	{
		raw = RuntimeInGameTextDictionary::TrimText(raw);
		if (!IsRawKey(raw))
		{
			return std::nullopt;
		}

		std::scoped_lock lock{ detail::g_aliasLock };
		if (const auto it = detail::g_keyToLiteral.find(std::wstring{ raw }); it != detail::g_keyToLiteral.end())
		{
			return it->second;
		}
		return std::nullopt;
	}
}
