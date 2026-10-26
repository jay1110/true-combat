/* Deterministic compatibility contracts, independent of game/engine startup. */
#include <limits.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "compat/compat_wrap.h"
#include "compat/compat_float.h"
#include "compat/compat_math.h"
#include "platform/platform_io.h"
/* Verify existing callers still reach the relocated implementations. */
#include "src/game/tce_numeric.h"
#include "src/game/tce_normalize_precision.h"
#include "src/game/tce_friction_precision.h"
#include "src/cgame/tce_bullet_precision.h"
#include "src/cgame/tce_weapon_fps_precision.h"

static int failures;
static int checks;

static void expect_int(const char *name, int actual, int expected)
{
	++checks;
	if (actual != expected)
	{
		fprintf(stderr, "%s: got %d, expected %d\n", name, actual, expected);
		++failures;
	}
}

static float float_from_bits(unsigned bits)
{
	float value;
	memcpy(&value, &bits, sizeof(value));
	return value;
}

static unsigned float_bits(float value)
{
	unsigned bits;
	memcpy(&bits, &value, sizeof(bits));
	return bits;
}

static void integer_goldens(void)
{
	expect_int("int32 low max", TCE_Int32FromUnsigned(0x7fffffffu), INT_MAX);
	expect_int("int32 low min", TCE_Int32FromUnsigned(0x80000000u), INT_MIN);
	expect_int("int32 low -1", TCE_Int32FromUnsigned(0xffffffffu), -1);
	expect_int("add overflow", TCE_AddInt32(INT_MAX, 1), INT_MIN);
	expect_int("add underflow", TCE_AddInt32(INT_MIN, -1), INT_MAX);
	expect_int("add two minima", TCE_AddInt32(INT_MIN, INT_MIN), 0);
	expect_int("subtract min", TCE_SubInt32(0, INT_MIN), INT_MIN);
	expect_int("subtract underflow", TCE_SubInt32(INT_MIN, 1), INT_MAX);
	expect_int("multiply low32", TCE_MulInt32(INT_MAX, 2), -2);
	expect_int("multiply min", TCE_MulInt32(INT_MIN, -1), INT_MIN);
	expect_int("multiply wrap zero", TCE_MulInt32(65536, 65536), 0);
	expect_int("float hundredth positive", TCE_Int32TimesFloatHundredth(100), 0);
	expect_int("float hundredth negative", TCE_Int32TimesFloatHundredth(-100), 0);
	expect_int("float hundredth threshold", TCE_Int32TimesFloatHundredth(101), 1);
}

static void conversion_goldens(void)
{
	float nanValue = float_from_bits(0x7fc01234u);
	float infinity = float_from_bits(0x7f800000u);
	expect_int("finite classification", TCE_FloatIsNaN(1.0f), 0);
	expect_int("nan classification", TCE_FloatIsNaN(nanValue), 1);
	expect_int("infinity classification", TCE_FloatIsNaN(infinity), 0);
	expect_int("negative zero", TCE_FloatIsZeroOrNaN(float_from_bits(0x80000000u)), 1);
	expect_int("nan zero-or-nan", TCE_FloatIsZeroOrNaN(nanValue), 1);
	expect_int("truncation toward zero", TCE_TruncateNativeInteger(-3.75L), -3);
#if defined(_WIN32)
	expect_int("windows signed64 low32", TCE_TruncateNativeInteger(4294967297.0L), 1);
	expect_int("windows negative signed64 low32", TCE_TruncateNativeInteger(-4294967297.0L), -1);
	expect_int("windows nan low word", TCE_TruncateNativeInteger((long double)nanValue), 0);
	expect_int("windows infinity low word", TCE_TruncateNativeInteger((long double)infinity), 0);
#else
	expect_int("nonwindows signed32 overflow", TCE_TruncateNativeInteger(4294967297.0L), INT_MIN);
	expect_int("nonwindows signed32 underflow", TCE_TruncateNativeInteger(-4294967297.0L), INT_MIN);
	expect_int("nonwindows nan indefinite", TCE_TruncateNativeInteger((long double)nanValue), INT_MIN);
	expect_int("nonwindows infinity indefinite", TCE_TruncateNativeInteger((long double)infinity), INT_MIN);
#endif
#if defined(_WIN32) || (defined(_MSC_VER) && defined(_M_IX86))
	expect_int("float signed64 positive low", TCE_FloatToInt64Low(4294967808.0f), 512);
	expect_int("float signed64 negative low", TCE_FloatToInt64Low(-4294967808.0f), -512);
	expect_int("float nan low", TCE_FloatToInt64Low(nanValue), 0);
#endif
}

static void math_goldens(void)
{
#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
	unsigned original, control, precision[3] = { _PC_24, _PC_53, _PC_64 };
	unsigned rounding[4] = { _RC_NEAR, _RC_DOWN, _RC_UP, _RC_CHOP };
	unsigned expected[4] = { 0x3eaaaaabu, 0x3eaaaaaau, 0x3eaaaaabu, 0x3eaaaaaau };
	unsigned large_ratio[4] = { 0x3f333333u, 0x3f333333u, 0x3f333334u, 0x3f333333u };
	unsigned negative_ratio[4] = { 0xbf333333u, 0xbf333334u, 0xbf333333u, 0xbf333333u };
	/* Derived independently from exact rational arithmetic: each multiply,
	 * subtract and divide is rounded to PC, then the result to binary32. */
	unsigned friction_ratio[3][4] = {
		{ 0x3f333334u, 0x3f333333u, 0x3f333333u, 0x3f333333u },
		{ 0x3f333333u, 0x3f333333u, 0x3f333334u, 0x3f333333u },
		{ 0x3f333333u, 0x3f333333u, 0x3f333334u, 0x3f333333u }
	};
	unsigned friction_scaled[3][4] = {
		{ 0x4249999au, 0x42499999u, 0x4249999au, 0x42499999u },
		{ 0x42499999u, 0x42499999u, 0x4249999bu, 0x42499999u },
		{ 0x42499999u, 0x42499999u, 0x4249999bu, 0x42499999u }
	};
	volatile double fps_inputs[7] = { 1.0, 10.0, 13.0, 16.0, 18.0, 20.0, 80.0 };
	int fps_expected[7] = { 1000, 100, 76, 62, 55, 50, 12 };
	volatile unsigned long long large_n = 0xc999998400000000ULL;
	volatile unsigned long long large_d = 0x14000000000000ULL;
	int p, r;
	__control87_2(0, 0, &original, NULL);
	for (p = 0; p < 3; ++p)
	{
		for (r = 0; r < 4; ++r)
		{
			float result = 0.0f;
			volatile float ratio = 0.0f;
			tceTriggerSum_t fixed;
			tceFrictionArithmetic_t friction;
			int fps, value;
			__control87_2(_MCW_EM | precision[p] | rounding[r],
			              _MCW_EM | _MCW_PC | _MCW_RC, &control, NULL);
			expect_int("masked finite profile", TCE_FiniteExceptionsMasked(control), 1);
			expect_int("finite product supported", TCE_FiniteProductStore(1.5f, 2.0f, &result), 1);
			expect_int("finite product value", (int)float_bits(result), (int)0x40400000u);
			result = TCE_RatioFloat32(1, 3, 0, 0, control);
			expect_int("ratio rounding", (int)float_bits(result), (int)expected[r]);
			/* Volatile inputs exercise optimized variable uint64 paths instead of
			 * allowing a compiler to replace the regression with constants. */
			expect_int("large numerator highest bit", TCE_RatioHighestBit(large_n), 63);
			expect_int("large denominator highest bit", TCE_RatioHighestBit(large_d), 52);
			fixed = TCE_RatioValue(1000, large_d, 49, 0, control);
			expect_int("weapon quotient float", (int)float_bits(TCE_RotateStore(fixed, control)), (int)0x42c80000u);
			expect_int("weapon quotient integer", TCE_FixedSigned64Low32(&fixed), 100);
			fixed = TCE_RatioValue(large_n, 9437184u, -41, 0, control);
			expect_int("large numerator ratio", (int)float_bits(TCE_RotateStore(fixed, control)), (int)large_ratio[r]);
			fixed = TCE_RatioValue(large_n, 9437184u, -41, 1, control);
			expect_int("negative large numerator ratio", (int)float_bits(TCE_RotateStore(fixed, control)), (int)negative_ratio[r]);
			for (fps = 0; fps < 7; ++fps)
			{
				value = -123;
				expect_int("weapon FPS supported", TCE_WeaponFrameLerp(fps_inputs[fps], &value), 1);
				expect_int("weapon FPS milliseconds", value, fps_expected[fps]);
			}
			TCE_FrictionBegin(&friction);
			TCE_FrictionGround(&friction, 6.0f, 0.05f, 100.0f, 72.0f);
			expect_int("friction fraction supported", TCE_FrictionFinish(&friction, 72.0f, 0, &ratio), 1);
			expect_int("friction fraction bits", (int)float_bits(ratio), (int)friction_ratio[p][r]);
			result = TCE_FrictionScale(ratio, 72.0f);
			expect_int("friction velocity bits", (int)float_bits(result), (int)friction_scaled[p][r]);
		}
	}
	__control87_2(original, _MCW_EM | _MCW_PC | _MCW_RC, &control, NULL);
#else
	/* Fixed-grid support has its original MSVC/x86 gate. No fallback parity claim. */
	expect_int("float storage bits", (int)float_bits(1.0f), (int)0x3f800000u);
#endif
}

static int captured_count;
static intptr_t captured[18];

#if defined(TCE_COMPAT_TEST_WASM_ABI) || defined(__EMSCRIPTEN__)
/* Emscripten uses the vector ABI; the optional host switch tests its packing
 * without providing or claiming a working WASM engine/runtime. */
#ifndef __EMSCRIPTEN__
#define __EMSCRIPTEN__ 1
#endif
static intptr_t capture_syscall(intptr_t *args)
{
	memcpy(captured, args, 17 * sizeof(*args));
	return 123;
}
static intptr_t (*syscall)(intptr_t *) = capture_syscall;
#else
static intptr_t capture_syscall(intptr_t op, ...)
{
	va_list args;
	int i;
	captured[0] = op;
	va_start(args, op);
	for (i = 1; i <= captured_count + 1; ++i)
	{
		captured[i] = va_arg(args, intptr_t);
	}
	va_end(args);
	return 123;
}
static intptr_t (*syscall)(intptr_t, ...) = capture_syscall;
#endif
#include "compat/compat_abi.h"
#include "src/game/tce_native_syscall.h"

static void abi_goldens(void)
{
	intptr_t pointer_slot = (intptr_t)&captured;
	int i;
	captured_count = 0;
	expect_int("syscall0 result", (int)TCE_SYSCALL_0(9), 123);
	expect_int("syscall0 opcode", (int)captured[0], 9);
#ifdef __EMSCRIPTEN__
	for (i = 1; i < 17; ++i)
	{
		expect_int("wasm zero-fill", (int)captured[i], 0);
	}
#else
	expect_int("native syscall0 sentinel", (int)captured[1], -1337);
#endif
	captured_count = 2;
	expect_int("syscall2 result", (int)TCE_SYSCALL_2(12, pointer_slot, -7), 123);
	expect_int("syscall2 opcode", (int)captured[0], 12);
	expect_int("pointer-sized syscall slot", captured[1] == pointer_slot, 1);
	expect_int("signed syscall slot", (int)captured[2], -7);
#ifdef __EMSCRIPTEN__
	for (i = 3; i < 17; ++i)
	{
		expect_int("wasm remaining zero-fill", (int)captured[i], 0);
	}
#else
	expect_int("native syscall2 sentinel", (int)captured[3], -1337);
#endif
	captured_count = 16;
	expect_int("syscall16 result", (int)TCE_SYSCALL_16(99, 1, 2, 3, 4, 5, 6, 7, 8,
	                                                   9, 10, 11, 12, 13, 14, 15, pointer_slot), 123);
	for (i = 1; i < 16; ++i)
	{
		expect_int("full syscall argument", (int)captured[i], i);
	}
	expect_int("full syscall pointer slot", captured[16] == pointer_slot, 1);
#ifndef __EMSCRIPTEN__
	expect_int("full syscall sentinel", (int)captured[17], -1337);
#endif
}

static unsigned listed_entries;
static int listing_errors;

static void directory_entry(void *context, const char *name, int directory)
{
	unsigned *entries = (unsigned *)context;
	if (!strcmp(name, ".") || !strcmp(name, ".."))
	{
		return;
	}
	if (!strcmp(name, "first.lua"))
	{
		*entries |= 1u;
		listing_errors += directory != 0;
	}
	else if (!strcmp(name, "second.txt"))
	{
		*entries |= 2u;
		listing_errors += directory != 0;
	}
	else if (!strcmp(name, "nested"))
	{
		*entries |= 4u;
		listing_errors += directory == 0;
	}
	else
	{
		++listing_errors;
	}
}

static void directory_goldens(const char *fixture)
{
	char scratch[4096];
	if (!fixture)
	{
		fprintf(stderr, "directory fixture argument is required\n");
		++failures;
		return;
	}
	expect_int("native directory opens",
	           TCE_PlatformListDirectory(fixture, scratch, sizeof(scratch),
	                                     directory_entry, &listed_entries), 1);
	expect_int("native directory membership", (int)listed_entries, 7);
	expect_int("native file/directory classification", listing_errors, 0);
	expect_int("null directory rejected",
	           TCE_PlatformListDirectory(NULL, scratch, sizeof(scratch),
	                                     directory_entry, &listed_entries), 0);
}

int main(int argc, char **argv)
{
	integer_goldens();
	conversion_goldens();
	math_goldens();
	abi_goldens();
	directory_goldens(argc > 1 ? argv[1] : NULL);
#if defined(_WIN32)
	expect_int("native OS separator", TCE_PLATFORM_NATIVE_PATH_SEPARATOR, '\\');
#else
	expect_int("native OS separator", TCE_PLATFORM_NATIVE_PATH_SEPARATOR, '/');
#endif
	printf("compat golden checks=%d failures=%d pointer_bits=%d\n",
	       checks, failures, (int)(8 * sizeof(void *)));
	return failures ? 1 : 0;
}
