#include "mn/SIMD.h"

// SIMD is only relevant for x86 family of architectures
#if ARCH_X86

#ifdef _MSC_VER
#include <intrin.h>
#endif

#ifdef __GNUC__
void __cpuid(int* cpuinfo, int info)
{
	__asm__ __volatile__(
		"xchg %%ebx, %%edi;"
		"cpuid;"
		"xchg %%ebx, %%edi;"
		:"=a" (cpuinfo[0]), "=D" (cpuinfo[1]), "=c" (cpuinfo[2]), "=d" (cpuinfo[3])
		:"0" (info)
	);
}

unsigned long long _xgetbv(unsigned int index)
{
	unsigned int eax, edx;
	__asm__ __volatile__(
		"xgetbv;"
		: "=a" (eax), "=d"(edx)
		: "c" (index)
	);
	return ((unsigned long long)edx << 32) | eax;
}
#endif

inline static mn_simd_support
_mn_simd_check()
{
	mn_simd_support res{};

	int cpuinfo[4];
	__cpuid(cpuinfo, 1);

	res.sse_supportted = cpuinfo[3] & (1 << 25) || false;
	res.sse2_supportted = cpuinfo[3] & (1 << 26) || false;
	res.sse3_supportted = cpuinfo[2] & (1 << 0) || false;
	res.ssse3_supportted = cpuinfo[2] & (1 << 9) || false;
	res.sse4_1_supportted = cpuinfo[2] & (1 << 19) || false;
	res.sse4_2_supportted = cpuinfo[2] & (1 << 20) || false;

	// Check AVX support
	// References
	// http://software.intel.com/en-us/blogs/2011/04/14/is-avx-enabled/
	// http://insufficientlycomplicated.wordpress.com/2011/11/07/detecting-intel-advanced-vector-extensions-avx-in-visual-studio/

	res.avx_supportted = cpuinfo[2] & (1 << 28) || false;
	bool osxsaveSupported = cpuinfo[2] & (1 << 27) || false;
	if (osxsaveSupported && res.avx_supportted)
	{
		// _XCR_XFEATURE_ENABLED_MASK = 0
		unsigned long long xcrFeatureMask = _xgetbv(0);
		res.avx_supportted = (xcrFeatureMask & 0x6) == 0x6;
	}

	// Check SSE4a and SSE5 support

	// Get the number of valid extended IDs
	__cpuid(cpuinfo, 0x80000000);
	int numExtendedIds = cpuinfo[0];
	if (numExtendedIds >= (int)0x80000001)
	{
		__cpuid(cpuinfo, 0x80000001);
		res.sse4a_supportted = cpuinfo[2] & (1 << 6) || false;
		res.sse5_supportted = cpuinfo[2] & (1 << 11) || false;
	}

	return res;
}
#else
inline static mn_simd_support
_mn_simd_check()
{
	mn_simd_support res{};
	return res;
}
#endif

mn_simd_support
mn_simd_support_check()
{
	static auto simd_support = _mn_simd_check();
	return simd_support;
}