#pragma once

#include <cstdint>
#include <xmmintrin.h>

namespace lpm
{
	constexpr size_t StrlenCompileTime(const char* const str)
	{
		size_t len = 0;
		while (str[len] != '\0')
		{
			++len;
		}

		return len;
	}

	constexpr unsigned long StrtoulCompileTime(const char* const str, char** endptr = nullptr, const int base = 16)
	{
		unsigned long result = 0;
		size_t i = 0;

		// Skip leading whitespaces.
		while (str[i] == ' ' || str[i] == '\t')
		{
			++i;
		}

		// Handle optional '0x' or '0X' prefix for hex.
		if (base == 16 && str[i] == '0' && (str[i + 1] == 'x' || str[i + 1] == 'X'))
		{
			i += 2;
		}

		const char* const start = str + i;
		while (str[i] != '\0')
		{
			const char c = str[i];
			uint32_t digit = 0;

			if (c >= '0' && c <= '9')
			{
				digit = c - '0';
			}
			else if (c >= 'a' && c <= 'f')
			{
				digit = c - 'a' + 10;
			}
			else if (c >= 'A' && c <= 'F')
			{
				digit = c - 'A' + 10;
			}
			else
			{
				// Invalid character for the given base if we reach this.
				break;
			}

			// Is digit is out of range for the given base?
			if (digit >= static_cast<unsigned int>(base))
			{
				break;
			}

			result = result * base + digit;
			++i;
		}

		// Skip forward if pointer provided on what is unprocessed.
		if (endptr != nullptr)
		{
			*endptr = const_cast<char*>(str[i] ? str + i : start);
		}

		return result;
	}

	class CPatternContainer
	{
	public:
		constexpr CPatternContainer(const char* const pattern, const char* const mask) : longestSeqFirstChar(), _pad(), pattern(pattern), mask(mask), maskLen(StrlenCompileTime(mask)), longestSeqStart(), longestSeqLen(), byteMask()
		{
			size_t currLongestSeqStart = 0;
			size_t currLongestSeqLen = 0;

			for (size_t i = 0; i < maskLen; ++i)
			{
				if (mask[i] != 'x')
					continue;

				// Get the size of the current sequence.
				size_t subSeqLen = 0;
				while (mask[i + subSeqLen] == 'x' && pattern[i + subSeqLen] != '\0')
				{
					++subSeqLen;
				}

				// Update the longest sequence if our current calculated one is bigger.
				if (subSeqLen > currLongestSeqLen)
				{
					currLongestSeqStart = i;
					currLongestSeqLen = subSeqLen;
				}

				i += subSeqLen - 1;
			}

			longestSeqFirstChar = static_cast<uint8_t>(pattern[currLongestSeqStart]);
			longestSeqStart = currLongestSeqStart;
			longestSeqLen = currLongestSeqLen;

			PopulateByteMask();
		}

	private:
		// Populating a byte mask for the longest sequence.
		// We will use this to skip over len of longest sequence if the current character in our data set is not a part of the mask.
		constexpr void PopulateByteMask()
		{
			for (size_t i = longestSeqStart; i < longestSeqStart + longestSeqLen; i++)
			{
				byteMask[static_cast<uint8_t>(pattern[i])] = 1;
			}
		}

	public:
		uint8_t longestSeqFirstChar;
		uint8_t _pad[7];

		const char* pattern;
		const char* mask;
		size_t maskLen;

		size_t longestSeqStart;
		size_t longestSeqLen;

		uint8_t byteMask[256];
	};

	#ifdef _M_IX86
		#define PATTERN_ITERRATOR_MAX_INT 0xffffffffui32
	#elif _M_X64
		#define PATTERN_ITERRATOR_MAX_INT 0xffffffffffffffffui64
	#endif // #ifdef _M_IX86
	uint8_t* FindPattern(const CPatternContainer& container, const uint8_t* const data, const size_t dataLen)
	{
		const char* const pattern = container.pattern;
		const char* const mask = container.mask;
		const size_t maskLen = container.maskLen;
		const size_t seqStart = container.longestSeqStart;
		const size_t seqLen = container.longestSeqLen;
		const uint8_t firstC = container.longestSeqFirstChar;
		const uint8_t* const byteMask = container.byteMask;

		for (size_t i = dataLen - maskLen; i != PATTERN_ITERRATOR_MAX_INT;)
		{
			_mm_prefetch(reinterpret_cast<const char*>(&data[i - 64]), _MM_HINT_T0);

			uint8_t c = data[i];
			uint8_t w = byteMask[c];

			// Check if the character matches the wildcard, if not continue walking till we hit one that is.
			while (w == 0 && i > seqLen)
			{
				i -= seqLen;
				c = data[i];
				w = byteMask[c];
			}

			// Check if the first character matches.
			if (c != firstC)
			{
				--i;
				continue;
			}

			// Bounds check for current sequence.
			const size_t delta = (i - seqStart);
			if (delta < 0 || delta + maskLen > dataLen)
			{
				return nullptr;
			}

			// Verify the rest of the pattern utilizing our mask.
			bool match = true;
			for (size_t j = 0; j < maskLen - 1; ++j)
			{
				if (mask[j] != 'x')
				{
					continue;
				}

				if (data[delta + j] != static_cast<uint8_t>(pattern[j]))
				{
					match = false;
					break;
				}
			}

			if (match)
			{
				return const_cast<uint8_t*>(data + delta);
			}

			--i;
		}

		return nullptr;
	}

	template<const char* S>
	class CIDAToCodePattern
	{
	public:
		constexpr CIDAToCodePattern() : codePattern(), mask()
		{
			// Grab start and end of pattern sequence.
			const char* idaPatternStart = S;
			const char* idaPatternEnd = S + StrlenCompileTime(S) - 1; // Last char before null terminator.

			size_t bufPos = 0;
			for (const char* currCh = idaPatternStart; currCh < idaPatternEnd; ++currCh)
			{
				// Skip whitespaces as we don't want them in our converted string.
				if (*currCh == ' ' || *currCh == '\t')
				{
					continue;
				}

				// Check for wildcard.
				if (*currCh == '?')
				{
					// Check for a second wildcard.
					++currCh;
					if (*currCh == '?')
					{
						codePattern[bufPos] = '\xAA'; // Using xAA as placeholder char.
						mask[bufPos] = '?';
						++bufPos;
						continue;
					}
					--currCh; // Go one char back if there wasn't another wildcard.

					codePattern[bufPos] = '\xAA';
					mask[bufPos] = '?';
					++bufPos;
					continue;
				}

				// Interpret as byte now.
				codePattern[bufPos] = static_cast<char>(StrtoulCompileTime(currCh, const_cast<char**>(&currCh), 16));
				mask[bufPos] = 'x';
				++bufPos;
			}
		}

		inline constexpr const char* GetPattern() const
		{
			return codePattern;
		}

		inline constexpr const char* GetMask() const
		{
			return mask;
		}

	private:
		constexpr static size_t CalcBufSize(const char* const str, const size_t cIdx = 0, const bool wasWildcard = false)
		{
			// We already account for null terminator in BufLen() so return 0.
			if (str[cIdx] == '\0')
			{
				return 0;
			}

			// If the current character is a space, skip it.
			if (str[cIdx] == ' ' || str[cIdx] == '\t')
			{
				return CalcBufSize(str, cIdx + 1, false);
			}

			// Check for wildcard
			if (str[cIdx] == '?')
			{
				// If we already had a wildcard, it's double '??' which counts as one wildcard, skip this character.
				if (wasWildcard)
				{
					return CalcBufSize(str, cIdx + 1, false);
				}

				// For handling single '?' we gotta count them as two characters, since the mask will insert a pseudo character.
				return CalcBufSize(str, cIdx + 1, true) + 2;
			}

			return CalcBufSize(str, cIdx + 1, str[cIdx] == '?') + 1;
		}

		constexpr static size_t BufLen()
		{
			// Actual size of the pattern that we end up getting + 1 for null terminator.
			return (CalcBufSize(S) / 2) + 1;
		}

		char codePattern[BufLen()];
		char mask[BufLen()];
	};
}

#define LPM_GET_PAT(name) lpm::name
#define LPM_DEF_PAT(pattern, name) \
namespace lpm \
{ \
	constexpr const char name##_ida_pattern[] = pattern; \
	constexpr CIDAToCodePattern<name##_ida_pattern> name##_code_pattern = CIDAToCodePattern<name##_ida_pattern>(); \
	constexpr CPatternContainer name = CPatternContainer(name##_code_pattern.GetPattern(), name##_code_pattern.GetMask()); \
}

#define LPM_FIND_PAT(container, data, dataLen) lpm::FindPattern(lpm::container, data, dataLen)