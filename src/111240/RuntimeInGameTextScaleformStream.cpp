// AI CONTEXT: Implements in-memory stream data for BSScaleformTranslator TXT loading.
// Depends on RuntimeInGameTextScaleformStream and standard byte copying.
// Runtime scope is Fallout 4 1.11.240 Scaleform translation dictionary injection.
// Version-specific logic: none; this only adapts memory to CommonLibF4 stream interfaces.
// Source-free policy: performs no translation identity decisions.
#include "PCH.h"

#include "111240/RuntimeInGameTextScaleformStream.h"

#include <algorithm>
#include <cstring>

namespace Runtime111240
{
namespace RuntimeInGameTextScaleformStream
{
	MemoryBinaryStream::MemoryBinaryStream(std::vector<std::uint8_t> data) :
		data_(std::move(data))
	{}

	MemoryBinaryStream::operator bool() const
	{
		return !data_.empty();
	}

	void MemoryBinaryStream::Seek(std::ptrdiff_t byteOffset)
	{
		if (byteOffset < 0)
		{
			const auto back = static_cast<std::size_t>(-byteOffset);
			position_ = back >= position_ ? 0 : position_ - back;
		}
		else
		{
			position_ = std::min<std::size_t>(data_.size(), position_ + static_cast<std::size_t>(byteOffset));
		}
		absoluteCurrentPos = position_;
	}

	std::size_t MemoryBinaryStream::GetPosition() const
	{
		return position_;
	}

	void MemoryBinaryStream::GetBufferInfo(BufferInfo& info)
	{
		info.buffer = data_.empty() ? nullptr : data_.data();
		info.fileSize = data_.size();
		info.bufferAllocSize = data_.size();
		info.bufferReadSize = data_.size();
		info.pos = position_;
		info.absCurrentPos = position_;
	}

	std::size_t MemoryBinaryStream::DoRead(void* buffer, std::size_t bytes)
	{
		if (!buffer || bytes == 0 || position_ >= data_.size())
		{
			return 0;
		}

		const auto toRead = std::min(bytes, data_.size() - position_);
		std::memcpy(buffer, data_.data() + position_, toRead);
		position_ += toRead;
		absoluteCurrentPos = position_;
		return toRead;
	}

	std::size_t MemoryBinaryStream::DoWrite(const void*, std::size_t)
	{
		return 0;
	}

	MemoryStreamParserData::MemoryStreamParserData(std::vector<std::uint8_t> data) :
		stream_(std::move(data))
	{}

	bool MemoryStreamParserData::Begin()
	{
		return static_cast<bool>(stream_);
	}

	void MemoryStreamParserData::End()
	{}

	RE::NiBinaryStream* MemoryStreamParserData::GetStream()
	{
		return std::addressof(stream_);
	}
}

} // namespace Runtime111240
