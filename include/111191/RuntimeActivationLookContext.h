// AI CONTEXT: Shares the latest activation-look context with activation translation lookups.
// Depends only on plain copied values; callers own live game pointer validation.
// Runtime scope is Fallout 4 1.11.191 activation/UI.
// Version-specific logic: none.
// Source-free policy: uses captured editorID as functional lookup identity.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace Runtime111191::RuntimeActivationLookContext
{
	struct Snapshot
	{
		std::uint64_t ageMs{ 0 };
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
		std::string_view text);

	[[nodiscard]] std::optional<Snapshot> LatestRecent(std::uint64_t maxAgeMs);
	[[nodiscard]] std::string FormatRecent(std::uint64_t maxAgeMs);
}
