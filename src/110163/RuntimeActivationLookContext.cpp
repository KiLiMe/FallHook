// AI CONTEXT: Stores and formats the latest activation-look context for UI trace correlation.
// Depends on Win32 tick count for cheap recency checks and copied diagnostic strings.
// Runtime scope is Fallout 4 1.10.163 activation/UI diagnostics.
// Version-specific logic: none.
// Source-free policy: diagnostic context only; live text is never used as lookup identity.
#include "PCH.h"

#include "110163/RuntimeActivationLookContext.h"

#include <Windows.h>

#ifdef ERROR
#	undef ERROR
#endif

#include <mutex>

namespace
{
	constexpr std::size_t kTextLimit{ 80 };

	struct StoredSnapshot
	{
		std::uint64_t tick{ 0 };
		std::uint64_t sequence{ 0 };
		std::uint32_t ref{ 0 };
		std::uint32_t base{ 0 };
		std::uint32_t baseLocal{ 0 };
		std::uint32_t refTypeID{ 0 };
		std::uint32_t baseTypeID{ 0 };
		std::string refType;
		std::string refEditor;
		std::string baseType;
		std::string baseEditor;
		std::string text;
		bool originalResult{ false };
	};

	std::mutex g_lock;
	StoredSnapshot g_latest;
	std::uint64_t g_sequence{ 0 };

	[[nodiscard]] std::string clip(std::string_view text)
	{
		std::string out{ text.substr(0, kTextLimit) };
		for (auto& ch : out)
		{
			if (ch == '\r' || ch == '\n' || ch == '\t')
			{
				ch = ' ';
			}
		}
		if (text.size() > kTextLimit)
		{
			out += "...";
		}
		return out;
	}
}

namespace RuntimeActivationLookContext
{
	void Update(
		std::uint32_t ref,
		std::uint32_t refTypeID,
		std::string_view refType,
		std::string_view refEditor,
		std::uint32_t base,
		std::uint32_t baseLocal,
		std::uint32_t baseTypeID,
		std::string_view baseType,
		std::string_view baseEditor,
		bool originalResult,
		std::string_view text)
	{
		std::scoped_lock lock{ g_lock };
		g_latest.tick = ::GetTickCount64();
		g_latest.sequence = ++g_sequence;
		g_latest.ref = ref;
		g_latest.base = base;
		g_latest.baseLocal = baseLocal;
		g_latest.refTypeID = refTypeID;
		g_latest.baseTypeID = baseTypeID;
		g_latest.refType = refType;
		g_latest.refEditor = refEditor;
		g_latest.baseType = baseType;
		g_latest.baseEditor = baseEditor;
		g_latest.originalResult = originalResult;
		g_latest.text = clip(text);
	}

	std::optional<Snapshot> LatestRecent(std::uint64_t maxAgeMs)
	{
		std::scoped_lock lock{ g_lock };
		if (g_latest.sequence == 0)
		{
			return std::nullopt;
		}
		const auto now = ::GetTickCount64();
		const auto age = now >= g_latest.tick ? now - g_latest.tick : 0;
		if (age > maxAgeMs)
		{
			return std::nullopt;
		}

		return Snapshot{
			.ageMs = age,
			.sequence = g_latest.sequence,
			.ref = g_latest.ref,
			.base = g_latest.base,
			.baseLocal = g_latest.baseLocal,
			.refTypeID = g_latest.refTypeID,
			.baseTypeID = g_latest.baseTypeID,
			.refType = g_latest.refType,
			.refEditor = g_latest.refEditor,
			.baseType = g_latest.baseType,
			.baseEditor = g_latest.baseEditor,
			.text = g_latest.text,
			.originalResult = g_latest.originalResult
		};
	}

	std::string FormatRecent(std::uint64_t maxAgeMs)
	{
		const auto snapshot = LatestRecent(maxAgeMs);
		if (!snapshot)
		{
			return {};
		}

		return std::format(
			"activationAge={} activationRef={:08X} activationRefType={} activationRefEditor=\"{}\" activationBase={:08X} activationBaseLocal={:06X} activationBaseType={} activationBaseEditor=\"{}\" activationResult={} activationText=\"{}\"",
			snapshot->ageMs,
			snapshot->ref,
			snapshot->refType,
			snapshot->refEditor,
			snapshot->base,
			snapshot->baseLocal,
			snapshot->baseType,
			snapshot->baseEditor,
			snapshot->originalResult,
			snapshot->text);
	}
}
