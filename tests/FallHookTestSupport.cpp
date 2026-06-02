// AI CONTEXT: Implements shared temporary-file and binary-record helpers for core tests.
// Depends on FallHookTestSupport declarations and zlib for compressed plugin test records.
// Runtime scope is Fallout 4 1.10.163 binary data-file semantics without loading the game.
// Version-specific logic: Fallout 4 1.10.163 only; no alternate runtime branches.
// Source-free policy: constructs source-free identity fixtures; does not implement text-key lookup.
#include "FallHookTestSupport.h"

#include <zlib.h>

#include <chrono>
#include <cstdlib>
#include <format>
#include <fstream>
#include <iostream>

namespace FallHookTestSupport
{
	void require(bool condition, std::string_view message)
	{
		if (!condition)
		{
			std::cerr << message << '\n';
			std::exit(1);
		}
	}

	std::filesystem::path writeTempFile(std::string_view suffix, std::string_view content)
	{
		const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		auto path = std::filesystem::temp_directory_path() / std::format("FallHook_{}_{}", stamp, suffix);
		std::ofstream out(path, std::ios::binary);
		out << content;
		return path;
	}

	std::filesystem::path writeTempBinary(std::string_view suffix, std::span<const std::uint8_t> content)
	{
		const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		auto path = std::filesystem::temp_directory_path() / std::format("FallHook_{}_{}", stamp, suffix);
		std::ofstream out(path, std::ios::binary);
		out.write(reinterpret_cast<const char*>(content.data()), static_cast<std::streamsize>(content.size()));
		return path;
	}

	void writeBinaryFile(const std::filesystem::path& path, std::span<const std::uint8_t> content)
	{
		std::ofstream out(path, std::ios::binary);
		out.write(reinterpret_cast<const char*>(content.data()), static_cast<std::streamsize>(content.size()));
	}

	void appendBytes(std::vector<std::uint8_t>& out, std::string_view bytes)
	{
		out.insert(out.end(), bytes.begin(), bytes.end());
	}

	void appendU16(std::vector<std::uint8_t>& out, std::uint16_t value)
	{
		out.push_back(static_cast<std::uint8_t>(value & 0xFF));
		out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
	}

	void appendU32(std::vector<std::uint8_t>& out, std::uint32_t value)
	{
		out.push_back(static_cast<std::uint8_t>(value & 0xFF));
		out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
		out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
		out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
	}

	void appendVector(std::vector<std::uint8_t>& out, const std::vector<std::uint8_t>& value)
	{
		out.insert(out.end(), value.begin(), value.end());
	}

	std::vector<std::uint8_t> bytesU16(std::uint16_t value)
	{
		std::vector<std::uint8_t> data;
		appendU16(data, value);
		return data;
	}

	std::vector<std::uint8_t> bytesU32(std::uint32_t value)
	{
		std::vector<std::uint8_t> data;
		appendU32(data, value);
		return data;
	}

	std::vector<std::uint8_t> subrecord(std::string_view signature, std::span<const std::uint8_t> data)
	{
		std::vector<std::uint8_t> out;
		appendBytes(out, signature);
		appendU16(out, static_cast<std::uint16_t>(data.size()));
		out.insert(out.end(), data.begin(), data.end());
		return out;
	}

	std::vector<std::uint8_t> subrecordString(std::string_view signature, std::string_view value)
	{
		std::vector<std::uint8_t> data(value.begin(), value.end());
		data.push_back('\0');
		return subrecord(signature, data);
	}

	std::vector<std::uint8_t> compressedPayload(std::span<const std::uint8_t> data)
	{
		std::vector<std::uint8_t> out;
		appendU32(out, static_cast<std::uint32_t>(data.size()));
		out.resize(4 + compressBound(static_cast<uLong>(data.size())));
		auto compressedSize = static_cast<uLongf>(out.size() - 4);
		const auto result = ::compress(out.data() + 4, &compressedSize, data.data(), static_cast<uLong>(data.size()));
		require(result == Z_OK, "test compression failed");
		out.resize(4 + compressedSize);
		return out;
	}

	std::vector<std::uint8_t> record(std::string_view signature, std::uint32_t rawFormID, const std::vector<std::uint8_t>& data, std::uint32_t flags)
	{
		std::vector<std::uint8_t> out;
		appendBytes(out, signature);
		appendU32(out, static_cast<std::uint32_t>(data.size()));
		appendU32(out, flags);
		appendU32(out, rawFormID);
		appendU32(out, 0);
		appendU32(out, 0);
		out.insert(out.end(), data.begin(), data.end());
		return out;
	}
}
