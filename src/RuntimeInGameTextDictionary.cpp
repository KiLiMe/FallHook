// AI CONTEXT: Loads and serves source-keyed TXT dictionary entries for in-game UI text.
// Depends on TxtTranslationParser, TextNormalization, Win32 UTF conversion, and filesystem scanning.
// Runtime assumptions: version-neutral dictionary storage loaded only by the selected module.
// Version-specific logic: none; hook offsets and UI sites live in version modules.
// Source-free policy: source lookup is confined to this approved TXT-only hook exception.
#include "PCH.h"

#include "RuntimeInGameTextDictionary.h"

#include "TextNormalization.h"
#include "TxtTranslationParser.h"

#include <Windows.h>

#ifdef ERROR
#	undef ERROR
#endif

#include <atomic>
#include <cstring>
#include <cwctype>
#include <mutex>
#include <unordered_map>

namespace
{
	std::mutex g_lock;
	std::unordered_map<std::wstring, std::wstring> g_map;
	std::atomic_uint64_t g_revision{ 0 };
	std::atomic_size_t g_entryCount{ 0 };

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

	std::filesystem::path dictionaryDirectory()
	{
		return gameDirectory() / "Data" / "F4SE" / "Plugins" / "FallHook";
	}

	int filenameRank(wchar_t ch) noexcept
	{
		if (ch >= L'0' && ch <= L'9')
		{
			return static_cast<int>(ch - L'0');
		}
		if (ch >= L'a' && ch <= L'z')
		{
			return 10 + static_cast<int>(ch - L'a');
		}
		if (ch >= L'A' && ch <= L'Z')
		{
			return 36 + static_cast<int>(ch - L'A');
		}
		return 62 + static_cast<int>(ch);
	}

	bool txtPathLess(const std::filesystem::path& lhs, const std::filesystem::path& rhs)
	{
		const auto lhsName = lhs.filename().wstring();
		const auto rhsName = rhs.filename().wstring();
		const auto count = std::min(lhsName.size(), rhsName.size());
		for (std::size_t i = 0; i < count; ++i)
		{
			const auto lhsRank = filenameRank(lhsName[i]);
			const auto rhsRank = filenameRank(rhsName[i]);
			if (lhsRank != rhsRank)
			{
				return lhsRank < rhsRank;
			}
			if (lhsName[i] != rhsName[i])
			{
				return lhsName[i] < rhsName[i];
			}
		}
		return lhsName.size() == rhsName.size() ? lhs.wstring() < rhs.wstring() : lhsName.size() < rhsName.size();
	}

	std::vector<std::filesystem::path> txtFiles(const std::filesystem::path& directory)
	{
		std::vector<std::filesystem::path> files;
		std::error_code ec;
		if (!std::filesystem::exists(directory, ec))
		{
			return files;
		}

		for (const auto& entry : std::filesystem::directory_iterator(directory, ec))
		{
			if (ec)
			{
				break;
			}
			if (!entry.is_regular_file(ec))
			{
				continue;
			}

			auto ext = entry.path().extension().wstring();
			std::ranges::transform(ext, ext.begin(), [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
			if (ext == L".txt")
			{
				files.emplace_back(entry.path());
			}
		}

		std::ranges::sort(files, txtPathLess);
		return files;
	}
}

namespace RuntimeInGameTextDictionary
{
	std::wstring Utf8ToWide(std::string_view input)
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

	std::string WideToUtf8(std::wstring_view input)
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

	std::wstring_view TrimText(std::wstring_view value) noexcept
	{
		while (!value.empty() && (value.back() == L'\0' || value.back() == L'\r' || value.back() == L'\n'))
		{
			value.remove_suffix(1);
		}
		while (!value.empty() && (value.front() == L' ' || value.front() == L'\t' || value.front() == L'\r' || value.front() == L'\n'))
		{
			value.remove_prefix(1);
		}
		while (!value.empty() && (value.back() == L' ' || value.back() == L'\t' || value.back() == L'\r' || value.back() == L'\n'))
		{
			value.remove_suffix(1);
		}
		return value;
	}

	std::wstring NormalizeKey(std::wstring_view value)
	{
		return std::wstring{ TrimText(value) };
	}

	bool ShouldCapture(std::wstring_view value) noexcept
	{
		value = TrimText(value);
		if (value.empty() || value.size() > 2048)
		{
			return false;
		}
		if (value.front() == L'$')
		{
			return true;
		}
		for (const auto ch : value)
		{
			if ((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') || ch > 0x7F)
			{
				return true;
			}
		}
		return false;
	}

	LoadStats LoadFromDisk(bool enabled)
	{
		LoadStats stats;
		stats.enabled = enabled;
		stats.directory = dictionaryDirectory();

		std::unordered_map<std::wstring, std::wstring> loaded;
		if (enabled)
		{
			for (const auto& file : txtFiles(stats.directory))
			{
				auto parsed = TxtTranslationParser::ParseFile(file);
				if (!parsed.success)
				{
					++stats.errors;
					REX::ERROR("{} InGameText TXT parse failed path='{}' error='{}'.", Plugin::NAME, file.string(), parsed.error);
					continue;
				}

				++stats.files;
				stats.parsedEntries += parsed.file.entries.size();
				for (const auto& entry : parsed.file.entries)
				{
					const auto source = NormalizeKey(Utf8ToWide(entry.source));
					const auto dest = Utf8ToWide(TextNormalization::NormalizeUtf8(entry.dest));
					if (source.empty() || dest.empty())
					{
						continue;
					}
					loaded[source] = dest;
				}
			}
		}

		stats.storedKeys = loaded.size();
		{
			std::scoped_lock lock{ g_lock };
			g_map = std::move(loaded);
			g_entryCount.store(g_map.size(), std::memory_order_release);
			g_revision.fetch_add(1, std::memory_order_acq_rel);
		}

		REX::INFO(
			"{} InGameText TXT dictionary enabled={} directory='{}' files={} parsed={} keys={} errors={}.",
			Plugin::NAME,
			enabled,
			stats.directory.string(),
			stats.files,
			stats.parsedEntries,
			stats.storedKeys,
			stats.errors);
		return stats;
	}

	std::uint64_t Revision() noexcept
	{
		return g_revision.load(std::memory_order_acquire);
	}

	std::size_t EntryCount() noexcept
	{
		return g_entryCount.load(std::memory_order_acquire);
	}

	std::optional<std::wstring> Lookup(std::wstring_view raw)
	{
		auto key = NormalizeKey(raw);
		if (key.empty())
		{
			return std::nullopt;
		}

		std::scoped_lock lock{ g_lock };
		if (const auto it = g_map.find(key); it != g_map.end())
		{
			return it->second;
		}
		if (key.front() == L'$' && key.size() > 1)
		{
			key.erase(key.begin());
			if (const auto it = g_map.find(key); it != g_map.end())
			{
				return it->second;
			}
		}
		return std::nullopt;
	}

	std::vector<std::uint8_t> BuildScaleformUtf16Txt(std::size_t& entryCount)
	{
		entryCount = 0;
		std::wstring content;
		content.push_back(L'\xFEFF');

		{
			std::scoped_lock lock{ g_lock };
			for (const auto& [key, dest] : g_map)
			{
				if (key.empty() || dest.empty())
				{
					continue;
				}
				content.append(key);
				content.push_back(L'\t');
				content.append(dest);
				content.append(L"\r\n");
				++entryCount;
			}
		}

		if (entryCount == 0)
		{
			return {};
		}

		std::vector<std::uint8_t> bytes(content.size() * sizeof(wchar_t));
		std::memcpy(bytes.data(), content.data(), bytes.size());
		return bytes;
	}
}
