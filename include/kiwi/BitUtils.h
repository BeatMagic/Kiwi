#pragma once
#include <cstdint>

#ifdef _MSC_VER
#include <intrin.h>
#pragma intrinsic(__popcnt)
#pragma intrinsic(__popcnt64)
#pragma intrinsic(_BitScanForward)
#pragma intrinsic(_BitScanForward64)
#pragma intrinsic(_BitScanReverse)
#pragma intrinsic(_BitScanReverse64)
#endif

#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
#include <cpuid.h>
#endif

#if defined(__SSE2__) || defined(__AVX2__)
	#include <immintrin.h>
#endif

namespace kiwi
{
	namespace utils
	{
		inline int countTrailingZeroes(uint32_t v)
		{
			if (v == 0)
			{
				return 32;
			}
#if defined(__GNUC__)
			return __builtin_ctz(v);
#elif defined(_MSC_VER)
			unsigned long count;
			_BitScanForward(&count, v);
			return (int)count;
#else
			// See Stanford bithacks, count the consecutive zero bits (trailing) on the
			// right with multiply and lookup:
			// http://graphics.stanford.edu/~seander/bithacks.html#ZerosOnRightMultLookup
			static const uint8_t tbl[32] = { 0,  1,  28, 2,  29, 14, 24, 3,  30, 22, 20,
											15, 25, 17, 4,  8,  31, 27, 13, 23, 21, 19,
											16, 7,  26, 12, 18, 6,  11, 5,  10, 9 };
			return (int)tbl[((uint32_t)((v & -v) * 0x077CB531U)) >> 27];
#endif
		}

		inline int countTrailingZeroes(uint64_t v)
		{
			if (v == 0)
			{
				return 64;
			}
#if defined(__GNUC__)
			return __builtin_ctzll(v);
#elif defined(_MSC_VER) && defined(_M_X64)
			unsigned long count;
			_BitScanForward64(&count, v);
			return (int)count;
#else
			return (uint32_t)v ? countTrailingZeroes((uint32_t)v)
				: 32 + countTrailingZeroes((uint32_t)(v >> 32));
#endif
		}

		inline int countLeadingZeroes(uint32_t v)
		{
			if (v == 0)
			{
				return 32;
			}
#if defined(__GNUC__)
			return __builtin_clz(v);
#elif defined(_MSC_VER)
			unsigned long count;
			_BitScanReverse(&count, v);
			// BitScanReverse gives the bit position (0 for the LSB, then 1, etc.) of the
			// first bit that is 1, when looking from the MSB. To count leading zeros, we
			// need to adjust that.
			return 31 - int(count);
#else
			// See Stanford bithacks, find the log base 2 of an N-bit integer in
			// O(lg(N)) operations with multiply and lookup:
			// http://graphics.stanford.edu/~seander/bithacks.html#IntegerLogDeBruijn
			static const uint8_t tbl[32] = { 31, 22, 30, 21, 18, 10, 29, 2,  20, 17, 15,
											13, 9,  6,  28, 1,  23, 19, 11, 3,  16, 14,
											7,  24, 12, 4,  8,  25, 5,  26, 27, 0 };
			v = v | (v >> 1);
			v = v | (v >> 2);
			v = v | (v >> 4);
			v = v | (v >> 8);
			v = v | (v >> 16);
			return (int)tbl[((uint32_t)(v * 0x07C4ACDDU)) >> 27];
#endif
		}

		inline int countLeadingZeroes(uint64_t v)
		{
			if (v == 0)
			{
				return 64;
			}
#if defined(__GNUC__)
			return __builtin_clzll(v);
#elif defined(_MSC_VER) && defined(_M_X64)
			unsigned long count;
			_BitScanReverse64(&count, v);
			return 63 - int(count);
#else
			return v >> 32 ? countLeadingZeroes((uint32_t)(v >> 32))
				: 32 + countLeadingZeroes((uint32_t)v);
#endif
		}

		inline int ceilLog2(uint32_t v) { return 32 - countLeadingZeroes(v - 1); }

		inline int ceilLog2(uint64_t v) { return 64 - countLeadingZeroes(v - 1); }


		// Software popcount implementation that doesn't require POPCNT instruction
		// Use this for SSE2-only code paths where POPCNT may not be available
		inline uint32_t popcountSoftware(uint32_t v)
		{
			v = v - ((v >> 1) & 0x55555555u);
			v = (v & 0x33333333u) + ((v >> 2) & 0x33333333u);
			return ((v + (v >> 4) & 0x0F0F0F0Fu) * 0x01010101u) >> 24;
		}

		inline uint64_t popcountSoftware(uint64_t v)
		{
			return popcountSoftware((uint32_t)(v & 0xFFFFFFFF)) + popcountSoftware((uint32_t)(v >> 32));
		}

		// Runtime detection for POPCNT support (cached)
		inline bool hasPopcnt()
		{
			static int cached = -1;
			if (cached < 0)
			{
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
				int cpuInfo[4];
				__cpuid(cpuInfo, 1);
				cached = (cpuInfo[2] & (1 << 23)) ? 1 : 0; // POPCNT is bit 23 of ECX
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
				unsigned int eax, ebx, ecx, edx;
				if (__get_cpuid(1, &eax, &ebx, &ecx, &edx))
				{
					cached = (ecx & (1 << 23)) ? 1 : 0;
				}
				else
				{
					cached = 0;
				}
#elif defined(__aarch64__) || defined(__arm__) || defined(_M_ARM) || defined(_M_ARM64)
				// ARM NEON always has equivalent of popcount via VCNT instruction
				cached = 1;
#else
				cached = 0;
#endif
			}
			return cached != 0;
		}

		// Runtime-dispatched popcount: uses hardware POPCNT if available, software fallback otherwise
		inline uint32_t popcountRuntime(uint32_t v)
		{
			if (hasPopcnt())
			{
#if defined(_MSC_VER)
				return __popcnt(v);
#elif defined(__GNUC__)
				return __builtin_popcount(v);
#endif
			}
			return popcountSoftware(v);
		}

		inline uint64_t popcountRuntime(uint64_t v)
		{
			if (hasPopcnt())
			{
#if defined(_MSC_VER) && defined(_M_X64)
				return __popcnt64(v);
#elif defined(__GNUC__)
				return __builtin_popcountll(v);
#endif
			}
			return popcountSoftware(v);
		}

		inline uint32_t popcount(uint32_t v)
		{
#if defined(__GNUC__)
			return __builtin_popcount(v);
#elif defined(_MSC_VER)
			return __popcnt(v);
#else
			return popcountSoftware(v);
#endif
		}

		inline uint64_t popcount(uint64_t v)
		{
#if defined(__GNUC__)
			return __builtin_popcountll(v);
#elif defined(_MSC_VER) && defined(_M_X64)
			return __popcnt64(v);
#else
			return popcountSoftware(v);
#endif
		}

#if defined(__APPLE__) || defined(EMSCRIPTEN)
		inline int countTrailingZeroes(size_t v) { return countTrailingZeroes((uint64_t)v); }
		
		inline int countLeadingZeroes(size_t v) { return countLeadingZeroes((uint64_t)v); }

		inline int ceilLog2(size_t v) { return ceilLog2((uint64_t)v); }

		inline size_t popcount(size_t v) { return popcount((uint64_t)v); }
#endif
	}
}

