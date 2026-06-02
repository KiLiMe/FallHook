// AI CONTEXT: Shared test helpers for FallHook core unit tests.
// Depends on the C++ standard library and binary helper conventions used by parser/index tests.
// Runtime scope is Fallout 4 1.10.163 data-file semantics without loading the game.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: helpers build target identity test data; no original-text lookup behavior lives here.
#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

namespace FallHookTestSupport
{
	void require(bool condition, std::string_view message);

	[[nodiscard]] std::filesystem::path writeTempFile(std::string_view suffix, std::string_view content);
	[[nodiscard]] std::filesystem::path writeTempBinary(std::string_view suffix, std::span<const std::uint8_t> content);
	void writeBinaryFile(const std::filesystem::path& path, std::span<const std::uint8_t> content);

	void appendBytes(std::vector<std::uint8_t>& out, std::string_view bytes);
	void appendU16(std::vector<std::uint8_t>& out, std::uint16_t value);
	void appendU32(std::vector<std::uint8_t>& out, std::uint32_t value);
	void appendVector(std::vector<std::uint8_t>& out, const std::vector<std::uint8_t>& value);

	[[nodiscard]] std::vector<std::uint8_t> bytesU16(std::uint16_t value);
	[[nodiscard]] std::vector<std::uint8_t> bytesU32(std::uint32_t value);
	[[nodiscard]] std::vector<std::uint8_t> subrecord(std::string_view signature, std::span<const std::uint8_t> data);
	[[nodiscard]] std::vector<std::uint8_t> subrecordString(std::string_view signature, std::string_view value);
	[[nodiscard]] std::vector<std::uint8_t> compressedPayload(std::span<const std::uint8_t> data);
	[[nodiscard]] std::vector<std::uint8_t> record(
		std::string_view signature,
		std::uint32_t rawFormID,
		const std::vector<std::uint8_t>& data,
		std::uint32_t flags = 0);
}
