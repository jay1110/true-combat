/* Bounded CPU microbenchmarks of the actual original and port DLLs.
 * Reuse the authenticated weapon-state fixture rather than recreating its
 * original global-state projection. No engine/renderer or code hooks. */
#include "../game/q_shared.h"
#include "../game/tce_bg.h"
#include "../cgame/tce_weapon_position.h"
#include <windows.h>

static tce_weaponDef_t *probe_weapon_def;
static void (__cdecl *probe_position)(tce_weaponPosition_t *, float *, float *);
static void (__cdecl *probe_angles_to_axis)(const float *, float (*)[3]);
#define weaponDef probe_weapon_def
#define TCE_CG_CalculateWeaponPosition probe_position
#define AnglesToAxis probe_angles_to_axis
#define main weapon_fixture_main
#include "tce_weapon_position_test.c"
#undef main

static void *mapped(HMODULE dll, const char *path, const char *symbol)
{
	FILE *file = fopen(path, "r");
	char line[2048], name[256];
	unsigned address, preferred = 0;
	if (!file) return NULL;
	while (fgets(line, sizeof(line), file))
	{
		if (sscanf(line, " Preferred load address is %x", &address) == 1)
		{
			preferred = address;
			continue;
		}
		if (sscanf(line, " %*s %255s %x", name, &address) == 2 && !strcmp(name, symbol))
		{
			fclose(file);
			return preferred && address >= preferred ? (byte *)dll + address - preferred : NULL;
		}
	}
	fclose(file);
	return NULL;
}

static void original_state(byte *base, const tce_weaponPosition_t *state, float *refdef)
{
	size_t i;
	for (i = 0; i < sizeof(mappings) / sizeof(mappings[0]); ++i)
		memcpy(base + mappings[i].address - 0x30000000, (const char *)state + mappings[i].offset, mappings[i].size);
	memcpy(refdef + 6, state->viewOrigin, 12);
	memcpy(refdef + 9, state->viewAxis, 36);
	*(void **)(base + 0x40863ac) = refdef;
	memcpy(base + 0x484af50 + state->weapon * 0xfd0, state->gunViewAngles, 12);
	memcpy(base + 0x4c1df20 + state->weapon * 0x1cc, &weaponDef[state->weapon], sizeof(tce_weaponDef_t));
	*(float *)(base + 0x48896c8) = state->developerAngles[0];
	*(float *)(base + 0x4012c28) = state->developerAngles[1];
	*(float *)(base + 0x488c968) = state->developerAngles[2];
}

int main(int argc, char **argv)
{
	HMODULE original, port;
	LARGE_INTEGER frequency, begin, end;
	void (__cdecl *native_position)(float *, float *);
	tce_weaponPosition_t state, actual;
	float refdef[18] = { 0 }, origin[3], angles[3];
	volatile float checksum = 0;
	int scene, repeat, i;
	const int iterations = 8192;
	if ((argc != 4 && argc != 5) || sizeof(void *) != 4) return 2;
	original = LoadLibraryA(argv[1]);
	port = LoadLibraryA(argv[2]);
	if (!original || !port) return 2;
	probe_position = mapped(port, argv[3], "_TCE_CG_CalculateWeaponPosition");
	probe_angles_to_axis = mapped(port, argv[3], "_AnglesToAxis");
	probe_weapon_def = mapped(port, argv[3], "_weaponDef");
	if (!probe_position || !probe_angles_to_axis || !probe_weapon_def) return 2;
	if (argc == 5 && !strcmp(argv[4], "--full-checks"))
	{
		int result = weapon_fixture_main(2, argv);
		FreeLibrary(port);
		FreeLibrary(original);
		return result;
	}
	native_position = (void *)((byte *)original + 0x73270);
	QueryPerformanceFrequency(&frequency);
	for (scene = 0; scene < 4; ++scene)
	{
		setup(&state, 11, 100 + scene * 301);
		state.developer = 0;
		/* Validate these actual-DLL paths with the existing state projection. */
		actual = state;
		compare((byte *)original, &actual);
		for (repeat = 0; repeat < 3; ++repeat)
		{
			original_state((byte *)original, &state, refdef);
			QueryPerformanceCounter(&begin);
			for (i = 0; i < iterations; ++i)
			{
				*(int *)((byte *)original + 0x407dea4) = state.time + i * 16;
				native_position(origin, angles);
			}
			QueryPerformanceCounter(&end);
			checksum += origin[0] + angles[0];
			printf("weapon_position scene=%d repeat=%d implementation=original calls=%d us_per_call=%.6f\n", scene, repeat,
			       iterations, (end.QuadPart - begin.QuadPart) * 1000000.0 / frequency.QuadPart / iterations);
			actual = state;
			QueryPerformanceCounter(&begin);
			for (i = 0; i < iterations; ++i)
			{
				actual.time = state.time + i * 16;
				probe_position(&actual, origin, angles);
			}
			QueryPerformanceCounter(&end);
			checksum += origin[0] + angles[0];
			printf("weapon_position scene=%d repeat=%d implementation=port calls=%d us_per_call=%.6f\n", scene, repeat,
			       iterations, (end.QuadPart - begin.QuadPart) * 1000000.0 / frequency.QuadPart / iterations);
		}
	}
	{
		static const char *components[] = {
			"_TCE_WeaponPositionProne", "_TCE_WeaponPositionLean", "_TCE_WeaponPositionSway"
		};
		int component;
		for (component = 0; component < 3; ++component)
		{
			void (__cdecl *function)(tce_weaponPosition_t *, float *, float *) = mapped(port, argv[3], components[component]);
			if (!function) continue;
			for (repeat = 0; repeat < 3; ++repeat)
			{
				setup(&actual, 11, 401);
				QueryPerformanceCounter(&begin);
				for (i = 0; i < 2048; ++i)
				{
					actual.time = 10000 + (i & 31);
					VectorCopy(actual.viewOrigin, origin);
					VectorCopy(actual.viewAngles, angles);
					function(&actual, origin, angles);
				}
				QueryPerformanceCounter(&end);
				checksum += origin[0] + angles[0];
				printf("component=%s repeat=%d calls=2048 us_per_call=%.6f\n", components[component], repeat,
				       (end.QuadPart - begin.QuadPart) * 1000000.0 / frequency.QuadPart / 2048);
			}
		}
	}
	printf("fixture_checks=%d checksum=%g (CPU fixture timing; no engine rendering or whole-frame FPS claim)\n", cases, checksum);
	FreeLibrary(port);
	FreeLibrary(original);
	return 0;
}
