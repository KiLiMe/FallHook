// AI CONTEXT: Implements the MESG control-markup preservation exception.
// Depends only on MessageIconFormatter declarations and standard algorithms.
// Runtime assumptions: version-neutral Fallout 4 MESG button/DESC control markup display preservation.
// Version-specific logic: none; runtime modules own executable-specific behavior.
// Source-free policy: AGENTS-approved MESG icon exception; runtime text is only a markup donor after source-free slot resolution.
// Exception note: do not remove as source-exact repair; this preserves runtime <> and [] control markup inside translated MESG text.
#include "MessageIconFormatter.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <vector>

namespace
{
	struct RuntimeMarkupSlot
	{
		std::string text;
	};

	struct ReplacementToken
	{
		std::size_t begin{ 0 };
		std::size_t end{ 0 };
	};

	[[nodiscard]] std::string lowercase(std::string_view value)
	{
		std::string result{ value };
		std::ranges::transform(result, result.begin(), [](unsigned char ch) {
			return static_cast<char>(std::tolower(ch));
		});
		return result;
	}

	[[nodiscard]] std::string openingTagName(std::string_view openingTag)
	{
		if (openingTag.size() < 3 || openingTag.front() != '<')
		{
			return {};
		}

		std::size_t begin = 1;
		while (begin < openingTag.size() && std::isspace(static_cast<unsigned char>(openingTag[begin])))
		{
			++begin;
		}
		if (begin < openingTag.size() && openingTag[begin] == '/')
		{
			return {};
		}

		std::size_t end = begin;
		while (end < openingTag.size())
		{
			const auto ch = openingTag[end];
			if (std::isspace(static_cast<unsigned char>(ch)) || ch == '>' || ch == '/' || ch == '=')
			{
				break;
			}
			++end;
		}

		return lowercase(openingTag.substr(begin, end - begin));
	}

	[[nodiscard]] bool isRichTextTagName(std::string_view name)
	{
		return name == "font" || name == "p" || name == "b" || name == "i" || name == "u" ||
			name == "br" || name == "div" || name == "span" || name == "a";
	}

	[[nodiscard]] bool isControllerFontSlot(std::string_view openingTag, std::string_view name)
	{
		if (name != "font")
		{
			return false;
		}

		const auto lowerOpening = lowercase(openingTag);
		return lowerOpening.find("$controller_buttons") != std::string::npos;
	}

	[[nodiscard]] bool isReplaceableAngleToken(std::string_view openingTag, std::string_view name)
	{
		if (isControllerFontSlot(openingTag, name))
		{
			return true;
		}
		if (isRichTextTagName(name))
		{
			return false;
		}

		return openingTag.find('=') != std::string_view::npos;
	}

	[[nodiscard]] std::optional<std::string> readAngleMarkupSlot(std::string_view text, std::size_t pos)
	{
		if (pos >= text.size() || text[pos] != '<')
		{
			return std::nullopt;
		}

		const auto openEnd = text.find('>', pos + 1);
		if (openEnd == std::string_view::npos)
		{
			return std::nullopt;
		}

		const auto openingTag = text.substr(pos, openEnd - pos + 1);
		const auto lowerOpeningTag = lowercase(openingTag);
		if (lowerOpeningTag.starts_with("<id=") || (lowerOpeningTag.size() > 1 && lowerOpeningTag[1] == '/'))
		{
			return std::nullopt;
		}

		const auto name = openingTagName(openingTag);
		if (name.empty())
		{
			return std::nullopt;
		}
		if (!isReplaceableAngleToken(openingTag, name))
		{
			return std::nullopt;
		}

		const auto lowerText = lowercase(text.substr(openEnd + 1));
		const auto closeTag = std::string{ "</" } + name + ">";
		const auto closeRelative = lowerText.find(closeTag);
		if (closeRelative != std::string::npos)
		{
			const auto closeEnd = openEnd + 1 + closeRelative + closeTag.size();
			return std::string{ text.substr(pos, closeEnd - pos) };
		}

		return std::string{ openingTag };
	}

	[[nodiscard]] std::vector<RuntimeMarkupSlot> collectRuntimeMarkupSlots(std::string_view runtimeText)
	{
		std::vector<RuntimeMarkupSlot> slots;
		for (std::size_t pos = 0; pos < runtimeText.size();)
		{
			if (auto markup = readAngleMarkupSlot(runtimeText, pos))
			{
				pos += markup->size();
				slots.push_back(RuntimeMarkupSlot{ std::move(*markup) });
				continue;
			}

			if (runtimeText[pos] == '[')
			{
				const auto end = runtimeText.find(']', pos + 1);
				if (end != std::string_view::npos)
				{
					slots.push_back(RuntimeMarkupSlot{ std::string{ runtimeText.substr(pos, end - pos + 1) } });
					pos = end + 1;
					continue;
				}
			}

			++pos;
		}

		return slots;
	}

	[[nodiscard]] std::vector<ReplacementToken> collectReplacementTokens(std::string_view text)
	{
		std::vector<ReplacementToken> tokens;
		for (std::size_t pos = 0; pos < text.size();)
		{
			if (auto markup = readAngleMarkupSlot(text, pos))
			{
				tokens.push_back(ReplacementToken{ pos, pos + markup->size() });
				pos += markup->size();
				continue;
			}

			if (text[pos] != '[')
			{
				++pos;
				continue;
			}

			const auto end = text.find(']', pos + 1);
			if (end == std::string_view::npos)
			{
				break;
			}

			tokens.push_back(ReplacementToken{ pos, end + 1 });
			pos = end + 1;
		}

		return tokens;
	}
}

namespace MessageIconFormatter
{
	ApplyResult ApplyRuntimeControlMarkup(std::string_view runtimeText, std::string_view translation)
	{
		ApplyResult result{ std::string{ translation }, 0 };

		const auto runtimeSlots = collectRuntimeMarkupSlots(runtimeText);
		if (runtimeSlots.empty())
		{
			return result;
		}

		const auto translationTokens = collectReplacementTokens(translation);
		if (translationTokens.empty())
		{
			return result;
		}

		result.text.clear();
		result.text.reserve(translation.size() + runtimeText.size());

		std::size_t cursor = 0;
		for (std::size_t i = 0; i < translationTokens.size(); ++i)
		{
			const auto& token = translationTokens[i];
			const auto translatedToken = translation.substr(token.begin, token.end - token.begin);
			result.text.append(translation.substr(cursor, token.begin - cursor));
			if (i < runtimeSlots.size())
			{
				const auto& slot = runtimeSlots[i];
				if (slot.text != translatedToken)
				{
					result.text.append(slot.text);
					++result.replaced;
				}
				else
				{
					result.text.append(translation.substr(token.begin, token.end - token.begin));
				}
			}
			else
			{
				++result.replaced;
			}
			cursor = token.end;
		}

		result.text.append(translation.substr(cursor));
		if (result.replaced == 0)
		{
			result.text = std::string{ translation };
		}

		return result;
	}
}
