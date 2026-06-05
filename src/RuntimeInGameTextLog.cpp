// AI CONTEXT: Implements exact-format raw logs for in-game UI text capture.
// Depends on RuntimeInGameTextDictionary UTF conversion and REX logging.
// Runtime assumptions: version-neutral UI diagnostics used by selected modules.
// Version-specific logic: none; hook modules own address IDs.
// Source-free policy: logging belongs only to the approved TXT source-key hook exception.
#include "PCH.h"

#include "RuntimeInGameTextLog.h"

#include "RuntimeInGameTextDictionary.h"

#include <Windows.h>

#ifdef ERROR
#	undef ERROR
#endif

#include <atomic>
#include <deque>
#include <filesystem>
#include <mutex>
#include <unordered_set>

namespace
{
	constexpr std::size_t kSeenLogLimit{ 8192 };
	constexpr wchar_t kLogKeySeparator{ 0x1F };

	std::atomic_bool g_logRaw{ false };
	std::mutex g_seenLock;
	std::unordered_set<std::wstring> g_seenLogLines;
	std::deque<std::wstring> g_seenLogOrder;

	std::string escapeUtf8(std::string_view value)
	{
		std::string out;
		out.reserve(value.size());
		for (const char ch : value)
		{
			switch (ch)
			{
			case '\\':
				out += "\\\\";
				break;
			case '"':
				out += "\\\"";
				break;
			case '\r':
				out += "\\r";
				break;
			case '\n':
				out += "\\n";
				break;
			case '\t':
				out += "\\t";
				break;
			default:
				out.push_back(ch);
				break;
			}
		}
		return out;
	}

	std::string escape(std::wstring_view value)
	{
		return escapeUtf8(RuntimeInGameTextDictionary::WideToUtf8(value));
	}

	void appendLogKey(std::wstring& key, std::wstring_view value)
	{
		key.push_back(kLogKeySeparator);
		key.append(RuntimeInGameTextDictionary::TrimText(value));
	}

	[[nodiscard]] bool rememberLogLine(
		wchar_t kind,
		std::wstring_view raw,
		std::wstring_view text,
		std::wstring_view dest = {})
	{
		std::wstring key;
		key.reserve(raw.size() + text.size() + dest.size() + 4);
		key.push_back(kind);
		appendLogKey(key, RuntimeInGameTextDictionary::NormalizeKey(raw));
		appendLogKey(key, text);
		appendLogKey(key, dest);

		std::scoped_lock lock{ g_seenLock };
		if (g_seenLogLines.contains(key))
		{
			return false;
		}
		if (g_seenLogOrder.size() >= kSeenLogLimit)
		{
			g_seenLogLines.erase(g_seenLogOrder.front());
			g_seenLogOrder.pop_front();
		}
		g_seenLogLines.insert(key);
		g_seenLogOrder.push_back(std::move(key));
		return true;
	}

	void clearDedupe()
	{
		std::scoped_lock lock{ g_seenLock };
		g_seenLogLines.clear();
		g_seenLogOrder.clear();
	}

	[[nodiscard]] std::string moduleOffset(const void* address)
	{
		if (!address)
		{
			return "0";
		}

		HMODULE module = nullptr;
		const auto flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
		if (!GetModuleHandleExA(flags, static_cast<LPCSTR>(address), std::addressof(module)) || !module)
		{
			return std::format("{:X}", reinterpret_cast<std::uintptr_t>(address));
		}

		char path[MAX_PATH]{};
		const auto length = GetModuleFileNameA(module, path, static_cast<DWORD>(std::size(path)));
		const auto name = length == 0 ? std::string{ "module" } : std::filesystem::path{ path }.filename().string();
		const auto offset = reinterpret_cast<std::uintptr_t>(address) - reinterpret_cast<std::uintptr_t>(module);
		return std::format("{}+{:X}", name, offset);
	}
}

namespace RuntimeInGameTextLog
{
	void Configure(bool logRaw) noexcept
	{
		g_logRaw.store(logRaw, std::memory_order_release);
		clearDedupe();
	}

	bool Enabled() noexcept
	{
		return g_logRaw.load(std::memory_order_acquire);
	}

	std::string FormatAddress(const void* address)
	{
		return moduleOffset(address);
	}

	void Hit(std::wstring_view raw, std::wstring_view text, std::wstring_view dest)
	{
		if (!Enabled())
		{
			return;
		}
		if (!rememberLogLine(L'H', raw, text, dest))
		{
			return;
		}

		const auto copy = RuntimeInGameTextDictionary::NormalizeKey(raw);
		REX::INFO(
			"[Lograw][HIT] copy=\"{}\" text=\"{}\" dest=\"{}\"",
			escape(copy),
			escape(RuntimeInGameTextDictionary::TrimText(text)),
			escape(RuntimeInGameTextDictionary::TrimText(dest)));
	}

	void Miss(std::wstring_view raw, std::wstring_view text)
	{
		if (!Enabled())
		{
			return;
		}
		if (!rememberLogLine(L'M', raw, text))
		{
			return;
		}

		const auto copy = RuntimeInGameTextDictionary::NormalizeKey(raw);
		REX::INFO(
			"[Lograw][MISS] copy=\"{}\" text=\"{}\"",
			escape(copy),
			escape(RuntimeInGameTextDictionary::TrimText(text)));
	}

}
