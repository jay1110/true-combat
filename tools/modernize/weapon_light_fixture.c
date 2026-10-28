#include "src/cgame/cg_local.h"
#include "src/cgame/tce_weapon_flash_color.h"

cg_t cg;
cgs_t cgs;
vmCvar_t cg_weaponLight;

static int checks, failures, lightCalls, flashCalls;
static float radius, intensity, rgb[3], origin[3];
static int lightFlags, lightShader;
static refEntity_t lastFlash;
static int traceCalls, traceSkip, traceMask;
static vec3_t traceStart, traceEnd;
static const float *traceMins, *traceMaxs;
static trace_t traceResult;

#define CHECK(x) do { ++checks; if (!(x)) { ++failures; fprintf(stderr, "line %d: %s\n", __LINE__, #x); } } while (0)

void trap_R_AddLightToScene(const vec3_t org, float r, float strength, float red, float green, float blue, qhandle_t shader, int flags)
{
	++lightCalls;
	radius = r; intensity = strength;
	rgb[0] = red; rgb[1] = green; rgb[2] = blue;
	memcpy(origin, org, sizeof(origin));
	lightFlags = flags; lightShader = shader;
}

void trap_R_AddRefEntityToScene(const refEntity_t *ent)
{
	++flashCalls;
	lastFlash = *ent;
}

void CG_Trace(trace_t *result, const vec3_t start, const vec3_t mins, const vec3_t maxs,
              const vec3_t end, int skipNumber, int mask)
{
	++traceCalls;
	memcpy(traceStart, start, sizeof(traceStart));
	memcpy(traceEnd, end, sizeof(traceEnd));
	traceMins = mins; traceMaxs = maxs;
	traceSkip = skipNumber; traceMask = mask;
	*result = traceResult;
}

/* ACTUAL_FUNCTIONS */

static weaponInfo_t weapon;
static centity_t cent;
static refEntity_t flash;
static playerState_t ps;

static void reset(void)
{
	memset(&cg, 0, sizeof(cg)); memset(&cgs, 0, sizeof(cgs));
	memset(&weapon, 0, sizeof(weapon)); memset(&cent, 0, sizeof(cent));
	memset(&flash, 0, sizeof(flash)); memset(&ps, 0, sizeof(ps));
	cg_weaponLight.integer = 1;
	cg.time = cent.muzzleFlashTime = 1000;
	cg.tceSparkIntensity = 1.f;
	cg.predictedPlayerState.clientNum = cent.currentState.number = 2;
	flash.hModel = 7;
	flash.origin[0] = 128.5f; flash.origin[1] = -44.25f; flash.origin[2] = 33.f;
	weapon.flashDlightColor[0] = 1.f;
	weapon.flashDlightColor[1] = .6f;
	weapon.flashDlightColor[2] = .23f;
	lightCalls = flashCalls = 0;
	traceCalls = 0;
	memset(&traceResult, 0, sizeof(traceResult));
	traceResult.fraction = 1.f;
	traceResult.entityNum = ENTITYNUM_NONE;
	cg.refdef.vieworg[0] = -17.f; cg.refdef.vieworg[1] = 12.f; cg.refdef.vieworg[2] = 54.f;
}

static void draw(int firstPerson, int weaponNum)
{
	ActualWeaponFlashTail(flash, &weapon, &cent, firstPerson ? &ps : NULL, weaponNum);
}

static float floatbits(unsigned bits)
{
	float result;
	memcpy(&result, &bits, sizeof(result));
	return result;
}

int main(void)
{
	int age, i, component;
	const int excluded[] = {4, 9, 1, 15, 55, 56, 26, 27, 28, 29, 30, 11, 61};
	const unsigned invalid[] = {0x7fc00000u, 0x7f800000u, 0xff800000u};
	float previous = 1.f;

	reset(); draw(1, 8);
	CHECK(flashCalls == 1 && lightCalls == 1);
	CHECK(radius == 192.f && intensity == .8f);
	CHECK(rgb[0] == 1.f && rgb[1] == .6f && rgb[2] == .23f);
	CHECK(!memcmp(origin, flash.origin, sizeof(origin)));
	CHECK(lightShader == 0 && lightFlags == 0);
	CHECK(traceCalls == 1 && traceMins == NULL && traceMaxs == NULL);
	CHECK(!memcmp(traceStart, cg.refdef.vieworg, sizeof(traceStart)));
	CHECK(!memcmp(traceEnd, flash.origin, sizeof(traceEnd)));
	CHECK(traceSkip == cg.predictedPlayerState.clientNum && traceMask == MASK_SOLID);
	CHECK(lastFlash.shaderRGBA[0] == 255 && lastFlash.shaderRGBA[1] == 255 &&
	      lastFlash.shaderRGBA[2] == 255 && lastFlash.shaderRGBA[3] == 255);

	/* The actual local world and view paths may both submit a flash; only the
	 * owning light contributes, in either order. Remote and third-person work. */
	reset(); draw(0, 8); draw(1, 8); CHECK(lightCalls == 1);
	reset(); draw(1, 8); draw(0, 8); CHECK(lightCalls == 1);
	reset(); cg.renderingThirdPerson = 1; draw(1, 8); draw(0, 8); CHECK(lightCalls == 1);
	reset(); cent.currentState.number = 3; draw(0, 8); CHECK(lightCalls == 1);
	reset(); cg.tcePortalScopeRendering = 1; draw(0, 8); draw(1, 8); CHECK(lightCalls == 0);
	reset(); cg_weaponLight.integer = 0; draw(1, 8); CHECK(flashCalls == 1 && lightCalls == 0);
	CHECK(traceCalls == 0);

	/* Exact CG_Trace service contract includes world and solid movers. Only a
	 * finite, fully clear ray may add light; the flash itself stays unchanged. */
	reset(); traceResult.entityNum = ENTITYNUM_WORLD; traceResult.fraction = .5f;
	draw(1, 8); CHECK(traceCalls == 1 && flashCalls == 1 && lightCalls == 0);
	reset(); cent.currentState.number = 3; traceResult.entityNum = 271; traceResult.fraction = .75f;
	draw(0, 8); CHECK(traceCalls == 1 && flashCalls == 1 && lightCalls == 0);
	reset(); cent.currentState.number = 3; draw(0, 8); CHECK(traceCalls == 1 && lightCalls == 1);
	reset(); traceResult.startsolid = qtrue; draw(1, 8); CHECK(traceCalls == 1 && lightCalls == 0);
	reset(); traceResult.allsolid = qtrue; draw(1, 8); CHECK(traceCalls == 1 && lightCalls == 0);
	reset(); traceResult.startsolid = traceResult.allsolid = qtrue;
	draw(1, 8); CHECK(traceCalls == 1 && lightCalls == 0);
	for (i = 0; i < sizeof(invalid)/sizeof(invalid[0]); ++i)
	{
		reset(); traceResult.fraction = floatbits(invalid[i]); draw(1, 8);
		CHECK(traceCalls == 1 && flashCalls == 1 && lightCalls == 0);
	}
	reset(); traceResult.fraction = .999999f; draw(1, 8); CHECK(lightCalls == 0);
	reset(); traceResult.fraction = 1.000001f; draw(1, 8); CHECK(lightCalls == 0);
	reset(); traceResult.fraction = -.01f; draw(1, 8); CHECK(lightCalls == 0);
	reset(); traceResult.fraction = 0.f; draw(1, 8); CHECK(lightCalls == 0);

	for (age = -2; age <= 55; ++age)
	{
		reset(); cg.time += age; draw(1, 8);
		CHECK(lightCalls == (age >= 0 && age < 30));
		CHECK(flashCalls == (age < 30)); /* Original flash timing preserved. */
		if (lightCalls)
		{
			CHECK(intensity > 0.f && intensity <= .8f && intensity < previous);
			previous = intensity;
		}
	}
	reset(); cent.muzzleFlashTime = 0; cg.time = 0; draw(1, 8); CHECK(lightCalls == 0);
	reset(); cent.muzzleFlashTime = 2147483640; cg.time = (-2147483647 - 1) + 2;
	draw(1, 8); CHECK(lightCalls == 1); /* Modular event age, no signed overflow. */
	reset(); cg.time += 40; draw(1, 46); CHECK(flashCalls == 0 && lightCalls == 0);
	reset(); cg.zoomTime = 900; cg.time += 40; draw(1, 46);
	CHECK(flashCalls == 1 && lightCalls == 0); /* Existing special flash lifetime. */

	/* A loaded suppressor media file can still contain a light color. A
	 * missing actual FP/TP flash model must always prevent both emissions. */
	reset(); flash.hModel = 0; draw(1, 8); draw(0, 8);
	CHECK(flashCalls == 0 && lightCalls == 0);
	CHECK(traceCalls == 0);
	for (i = 0; i < sizeof(excluded)/sizeof(excluded[0]); ++i)
	{
		reset(); draw(1, excluded[i]); CHECK(flashCalls == 0 && lightCalls == 0);
	}
	reset(); draw(1, 66); CHECK(flashCalls == 0 && lightCalls == 0);
	reset(); cent.currentState.eFlags = EF_DEAD; draw(1, 8); CHECK(lightCalls == 0);
	reset(); cgs.gamestate = GS_INTERMISSION; draw(1, 8); CHECK(lightCalls == 0);
	reset(); cent.currentState.number = -1; draw(1, 8); CHECK(lightCalls == 0);
	reset(); cent.currentState.number = MAX_CLIENTS; draw(0, 8); CHECK(lightCalls == 0);
	for (component = 0; component < 3; ++component)
	{
		for (i = 0; i < sizeof(invalid)/sizeof(invalid[0]); ++i)
		{
			reset(); flash.origin[component] = floatbits(invalid[i]); draw(1, 8); CHECK(lightCalls == 0);
			reset(); weapon.flashDlightColor[component] = floatbits(invalid[i]); draw(1, 8); CHECK(lightCalls == 0);
		}
		reset(); flash.origin[component] = MAX_MAP_SIZE+1.f; draw(1, 8); CHECK(lightCalls == 0);
		reset(); flash.origin[component] = -MAX_MAP_SIZE-1.f; draw(1, 8); CHECK(lightCalls == 0);
		reset(); weapon.flashDlightColor[component] = -.01f; draw(1, 8); CHECK(lightCalls == 0);
		reset(); weapon.flashDlightColor[component] = 1.01f; draw(1, 8); CHECK(lightCalls == 0);
	}
	reset(); memset(weapon.flashDlightColor, 0, sizeof(weapon.flashDlightColor)); draw(1, 8); CHECK(lightCalls == 0);
	printf("weapon light: %d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
