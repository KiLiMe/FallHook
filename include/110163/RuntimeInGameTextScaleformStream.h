// AI CONTEXT: Exposes an in-memory BSStreamParserData adapter for Scaleform TXT injection.
// Depends on CommonLibF4 NiBinaryStream/BSStreamParserData and byte vectors.
// Runtime scope is Fallout 4 1.10.163 BSScaleformTranslator AddTranslations input only.
// Version-specific logic: none; hook modules own 1.10.163 relocation IDs.
// Source-free policy: transports already-approved TXT source-key entries; performs no lookup.
#pragma once

#include "RE/B/BSStreamParserData.h"
#include "RE/N/NiBinaryStream.h"

#include <cstdint>
#include <vector>

namespace RuntimeInGameTextScaleformStream
{
	class MemoryBinaryStream final :
		public RE::NiBinaryStream
	{
	public:
		explicit MemoryBinaryStream(std::vector<std::uint8_t> data);

		explicit operator bool() const override;
		void Seek(std::ptrdiff_t byteOffset) override;
		std::size_t GetPosition() const override;
		void GetBufferInfo(BufferInfo& info) override;
		std::size_t DoRead(void* buffer, std::size_t bytes) override;
		std::size_t DoWrite(const void* buffer, std::size_t bytes) override;

	private:
		std::vector<std::uint8_t> data_;
		std::size_t position_{ 0 };
	};

	class MemoryStreamParserData final :
		public RE::BSStreamParserData
	{
	public:
		explicit MemoryStreamParserData(std::vector<std::uint8_t> data);

		bool Begin() override;
		void End() override;
		RE::NiBinaryStream* GetStream() override;

	private:
		MemoryBinaryStream stream_;
	};
}
