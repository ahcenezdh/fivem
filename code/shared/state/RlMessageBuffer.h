#pragma once

#include <Span.h>

namespace rl
{
class MessageBufferLengthHack
{
public:
	static bool GetState();
};

template<typename BufferType>
class MessageBufferBase
{
public:
	inline MessageBufferBase()
		: m_curBit(0), m_maxBit(0)
	{

	}

	inline MessageBufferBase(const BufferType& data)
		: m_data(data), m_curBit(0), m_maxBit(data.size() * 8)
	{
		
	}

	inline MessageBufferBase(BufferType&& data)
		: m_data(std::move(data)), m_curBit(0), m_maxBit(m_data.size() * 8)
	{

	}

	inline explicit MessageBufferBase(size_t size)
		: m_data(size), m_curBit(0), m_maxBit(size * 8)
	{

	}

	inline MessageBufferBase(const void* data, size_t size)
		: m_data(reinterpret_cast<const uint8_t*>(data), reinterpret_cast<const uint8_t*>(data) + size), m_curBit(0), m_maxBit(size * 8)
	{

	}

	/// <summary>
	/// rage::datBitBuffer::ReadUnsigned = 0x14128DCF8 (1604)
	/// </summary>
	template<typename T>
	inline bool ReadBitsSingle(T* out, int length)
	{
		if (length == 13 && MessageBufferLengthHack::GetState())
		{
			length = 16;
		}

		static_assert(std::is_integral_v<T>, "ReadBitsSingle wants an int value");

		if ((m_curBit + length) > m_maxBit)
		{
			m_curBit += length;
			return false;
		}

		int startIdx = m_curBit / 8;
		int shift = m_curBit % 8;

		uint32_t retval = (uint8_t)(m_data[startIdx] << shift);
		startIdx++;

		if (length > 8)
		{
			int remaining = ((length - 9) / 8) + 1;

			while (remaining > 0)
			{
				uint32_t thisVal = (uint32_t)(m_data[startIdx] << shift);
				startIdx++;

				retval = thisVal | (retval << 8);

				remaining--;
			}
		}

		if (shift)
		{
			auto leftover = (startIdx < m_data.size()) ? m_data[startIdx] : 0;
			retval |= (uint8_t)(leftover >> (8 - shift));
		}

		retval = retval >> (((length + 7) & 0xF8) - length);

		m_curBit += length;

		// hack to prevent an out-of-bounds write of `out`
		*out = *(T*)&retval;

		return true;
	}


	inline uint8_t ReadBit()
	{
		int startIdx = m_curBit / 8;

		if (startIdx >= m_data.size())
		{
			return 0;
		}

		int shift = (7 - (m_curBit % 8));
		uint32_t retval = (uint8_t)(m_data[startIdx] >> shift);

		m_curBit++;

		return (uint8_t)(retval & 1);
	}

	inline void WriteBitsOld(const void* data, int length)
	{
		if (length == 13)
		{
			length = 16;
		}

		auto byteData = (const uint8_t*)data;

		for (int i = 0; i < length; i++)
		{
			int startIdx = i / 8;
			int shift = (7 - (i % 8));

			WriteBit((byteData[startIdx] >> shift) & 1);
		}
	}
	
	inline bool CopyBits(const void* dest, const void* source, int length, int destBitOffset, int sourceBitOffset)
	{
		const uint8_t* src = static_cast<const uint8_t*>(source) + (sourceBitOffset >> 3);
		uint8_t* dst = static_cast<uint8_t*>(const_cast<void*>(dest)) + (destBitOffset >> 3);
    
		int srcBitOffset = sourceBitOffset & 7;
		int dstBitOffset = destBitOffset & 7;
		int bitsLeft = length;

		// Handle initial unaligned bits
		if (srcBitOffset != 0) {
			int bitsToCopy = std::min(8 - srcBitOffset, bitsLeft);
			uint8_t mask = (1 << bitsToCopy) - 1;
			uint8_t value = (*src >> (8 - srcBitOffset - bitsToCopy)) & mask;
        
			*dst = (*dst & ~(mask << (8 - dstBitOffset - bitsToCopy))) | (value << (8 - dstBitOffset - bitsToCopy));
        
			src++;
			bitsLeft -= bitsToCopy;
			dstBitOffset = (dstBitOffset + bitsToCopy) & 7;
			if (dstBitOffset == 0) dst++;
		}

		// Copy whole bytes
		while (bitsLeft >= 8) {
			if (dstBitOffset == 0) {
				*dst = *src;
			} else {
				*dst = (*dst & ((1 << dstBitOffset) - 1)) | (*src << dstBitOffset);
				dst++;
				*dst = (*dst & ~((1 << dstBitOffset) - 1)) | (*src >> (8 - dstBitOffset));
			}
			src++;
			dst++;
			bitsLeft -= 8;
		}

		// Handle remaining bits
		if (bitsLeft > 0) {
			uint8_t mask = (1 << bitsLeft) - 1;
			uint8_t value = *src & mask;
        
			*dst = (*dst & ~(mask << (8 - dstBitOffset - bitsLeft))) | (value << (8 - dstBitOffset - bitsLeft));
		}

		return true;
	}
	
	inline bool ReadBits(void* data, int length)
	{
		if (length == 0)
		{
			return true;
		}

		if ((m_curBit + length) > m_maxBit)
		{
			return false;
		}

		const bool success = CopyBits(data, m_data.data(), length, 0, m_curBit);
    
		if (success)
		{
			m_curBit += length;
		}

		return success;
	}
	
	inline bool WriteBits(const void* data, int length)
	{
		if (length == 0)
		{
			return true;
		}

		if ((m_curBit + length) > m_maxBit)
		{
			return false;
		}

		const bool success = CopyBits(m_data.data(), data, length, m_curBit, 0);
    
		if (success)
		{
			m_curBit += length;
		}

		return success;
	}

	template<typename T>
	inline bool WriteBitsSingle(const T* data, int length)
	{
		static_assert(std::is_integral_v<T>, "WriteBitsSingle wants an int value");

		if (length == 13 && MessageBufferLengthHack::GetState())
		{
			length = 16;
		}

		if (length <= 0 || length > sizeof(T) * 8)
		{
			return false;  // Invalid length
		}

		if ((m_curBit + length) > m_maxBit)
		{
			return false;  // Not enough space in buffer
		}

		uint32_t value = static_cast<uint32_t>(*data);
		uint32_t mask = (length == 32) ? 0xFFFFFFFF : ((1U << length) - 1);
		value &= mask;  // Ensure we only write the specified number of bits

		int byteIndex = m_curBit / 8;
		int bitOffset = m_curBit % 8;

		while (length > 0)
		{
			int bitsToWrite = std::min(8 - bitOffset, length);
			uint8_t byteMask = static_cast<uint8_t>((1 << bitsToWrite) - 1) << (8 - bitOffset - bitsToWrite);
			uint8_t byteValue = static_cast<uint8_t>(value >> (length - bitsToWrite)) << (8 - bitOffset - bitsToWrite);

			m_data[byteIndex] = (m_data[byteIndex] & ~byteMask) | (byteValue & byteMask);

			length -= bitsToWrite;
			bitOffset = 0;
			++byteIndex;
		}

		m_curBit += (length == 16) ? 13 : length;  // it's for MessageBufferLengthHack
		return true;
	}

	inline bool WriteBit(uint8_t bit)
	{
		int startIdx = m_curBit / 8;

		if (startIdx >= m_data.size())
		{
			return false;
		}

		int shift = (7 - (m_curBit % 8));
		m_data[startIdx] = (m_data[startIdx] & ~(1 << shift)) | (bit << shift);

		m_curBit++;

		return true;
	}

	inline bool RequireLength(int length)
	{
		return ((m_curBit + length) < m_maxBit);
	}

	template<typename T>
	inline T Read(int length)
	{
		static_assert(sizeof(T) <= 4, "maximum of 32 bit read");

		uint32_t val = 0;
		ReadBitsSingle(&val, length);

		return T(val);
	}


	template<typename T>
	inline bool Read(int length, T* out)
	{
		static_assert(sizeof(T) <= 4, "maximum of 32 bit read");

		uint32_t val = 0;
		const bool success = ReadBitsSingle(&val, length);

		if (success)
		{
			*out = T(val);
		}
		else
		{
			*out = T();
		}

		return success;
	}

	template<typename T>
	inline T ReadSigned(int length)
	{
		int sign = Read<int>(1);
		int data = Read<int>(length - 1);

		return T{ sign + (data ^ -sign) };
	}

	template<typename T>
	inline void Write(int length, T data)
	{
		static_assert(sizeof(T) <= 4, "maximum of 32 bit write");

		WriteBitsSingle(&data, length);
	}

	template<typename T>
	inline void WriteSigned(int length, T data)
	{
		int sign = data < 0;
		int signEx = (data < 0) ? 0xFFFFFFFF : 0;
		int d = (data ^ signEx);

		Write<int>(1, sign);
		Write<int>(length - 1, d);
	}

	inline float ReadFloat(int length, float divisor)
	{
		auto integer = Read<int>(length);

		float max = (1 << length) - 1;
		return ((float)integer / max) * divisor;
	}

	inline void WriteFloat(int length, float divisor, float value)
	{
		float max = (1 << length) - 1;
		int integer = (int)((value / divisor) * max);

		Write<int>(length, integer);
	}

	inline float ReadSignedFloat(int length, float divisor)
	{
		auto integer = ReadSigned<int>(length);

		float max = (1 << (length - 1)) - 1;
		return ((float)integer / max) * divisor;
	}

	inline void WriteSignedFloat(int length, float divisor, float value)
	{
		float max = (1 << (length - 1)) - 1;
		int integer = (int)((value / divisor) * max);

		WriteSigned<int>(length, integer);
	}

	inline uint64_t ReadLong(int length)
	{
		if (length <= 32)
		{
			return Read<uint32_t>(length);
		}
		else
		{
			return Read<uint32_t>(32) | (((uint64_t)Read<uint32_t>(length - 32)) << 32);
		}
	}

	inline MessageBufferBase Clone()
	{
		auto s = m_maxBit - std::min(m_curBit, m_maxBit);
		auto c = (s / 8) + (s % 8 != 0) ? 1 : 0;

		std::vector<uint8_t> newData(c);
		ReadBits(newData.data(), s);
		return MessageBufferBase{newData};
	}

	inline void Align()
	{
		int r = m_curBit % 8;

		if (r != 0)
		{
			m_curBit += (8 - r);
		}
	}

	inline uint32_t GetCurrentBit()
	{
		return m_curBit;
	}

	inline void SetCurrentBit(uint32_t bit)
	{
		m_curBit = bit;
	}

	inline bool IsAtEnd()
	{
		return m_curBit >= m_maxBit;
	}

	inline BufferType& GetBuffer()
	{
		return m_data;
	}

	inline size_t GetLength()
	{
		return m_data.size();
	}

	inline size_t GetDataLength()
	{
		char leftoverBit = (m_curBit % 8) ? 1 : 0;

		return (m_curBit / 8) + leftoverBit;
	}

private:
	BufferType m_data;
	int m_curBit;
	int m_maxBit;
};

typedef MessageBufferBase<std::vector<uint8_t>> MessageBuffer;
typedef MessageBufferBase<net::Span<uint8_t>> MessageBufferView;
}
