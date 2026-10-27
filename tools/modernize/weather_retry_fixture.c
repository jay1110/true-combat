#include "src/cgame/cg_local.h"
#include "src/game/tce_numeric.h"
#include "src/game/tce_weather_precision.h"
#include "src/game/tce_tracemap_precision.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#undef vsnprintf
int Q_vsnprintf(char *dest, int size, const char *format, va_list args) { return vsnprintf(dest, size, format, args); }
#endif

#define ATM_NEW
#define MAX_ATMOSPHERIC_PARTICLES 4000
#define MAX_ATMOSPHERIC_EFFECTSHADERS 6
#define MAX_ATMOSPHERIC_DISTANCE 1000
#define MAX_ATMOSPHERIC_HEIGHT MAX_MAP_SIZE
#define ATMOSPHERIC_DROPDELAY 1000
#define MAX_WORLD_HEIGHT MAX_MAP_SIZE
#define MIN_WORLD_HEIGHT (-MAX_MAP_SIZE)
#define TRACEMAP_SIZE 256

cg_t cg;
vmCvar_t cg_atmosphericEffects, developer;
static unsigned char *traceData;
static size_t traceSize, tracePosition;
static int checks, failures, generates, generationFailures, renders, visibilityChecks, forceFailure;

void QDECL Com_Error(int level, const char *format, ...) { (void)level; (void)format; abort(); }
void QDECL Com_Printf(const char *format, ...) { (void)format; }
int trap_FS_FOpenFile(const char *name, fileHandle_t *file, fsMode_t mode)
{
    (void)name; if (mode != FS_READ) abort(); tracePosition = 0; *file = 1; return (int)traceSize;
}
void trap_FS_Read(void *dest, int len, fileHandle_t file)
{
    (void)file; if (len < 0 || (size_t)len > traceSize-tracePosition) abort();
    memcpy(dest, traceData+tracePosition, len); tracePosition += len;
}
void trap_FS_FCloseFile(fileHandle_t file) { (void)file; }

/* PRODUCTION_TYPES */

/* PRODUCTION_TRACEMAP_FUNCTIONS */

/* PRODUCTION_PARTICLE_FUNCTIONS */

static qboolean CG_EffectGustCurrent(vec3_t vector, float *weight, int *count)
{
    vector[0] = vector[1] = 50; vector[2] = -80;
    *weight = 1; *count = cg_atmFx.numDrops; return qfalse;
}
static void CG_EffectGust(void) {}

/* PRODUCTION_WEATHER_LOOP */

static void init_deadlines(void)
{
    int count;
    /* PRODUCTION_INITIAL_DEADLINES */
}
static qboolean active(cg_atmosphericParticle_t *particle)
{
    ++visibilityChecks;
    if (particle->active) { particle->pos[0] += 1; return qtrue; }
    return qfalse;
}
static qboolean generate(cg_atmosphericParticle_t *particle, vec3_t vector, float weight)
{
    (void)vector; (void)weight; ++generates;
    if (forceFailure) return qfalse;
    particle->active = ACT_FALLING; return qtrue;
}
static qboolean real_generate(cg_atmosphericParticle_t *particle, vec3_t vector, float weight)
{
    qboolean result;
    ++generates; result = CG_SnowParticleGenerate(particle, vector, weight);
    if (!result) ++generationFailures;
    return result;
}
static void render(cg_atmosphericParticle_t *particle) { (void)particle; ++renders; }
static void setup(int timeValue, int count)
{
    memset(&cg_atmFx, 0, sizeof(cg_atmFx)); memset(&cg, 0, sizeof(cg));
    cg.refdef_current = &cg.refdef; cg.time = timeValue;
    cg_atmFx.lastRainTime = timeValue; cg_atmFx.numDrops = count;
    cg_atmFx.numEffectShaders = 1; cg_atmosphericEffects.value = 1;
    cg_atmFx.ParticleCheckVisible = active; cg_atmFx.ParticleGenerate = generate;
    cg_atmFx.ParticleRender = render;
    generates = generationFailures = renders = visibilityChecks = 0; forceFailure = 1;
}
#define CHECK(c) do { ++checks; if (!(c)) { if (++failures <= 12) fprintf(stderr, "line %d: %s\n", __LINE__, #c); } } while (0)
static double seconds(void)
{
#if defined(_WIN32)
    LARGE_INTEGER now, frequency; QueryPerformanceCounter(&now); QueryPerformanceFrequency(&frequency);
    return (double)now.QuadPart/frequency.QuadPart;
#else
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now); return now.tv_sec+now.tv_nsec*1e-9;
#endif
}
static unsigned particle_state_hash(void)
{
    unsigned hash=2166136261u;
    int i;
    for(i=0;i<cg_atmFx.numDrops;++i) {
        const cg_atmosphericParticle_t *p=&cg_atmFx.particles[i];
        const unsigned char *bytes=(const unsigned char *)p;
        size_t j;
        unsigned shader=p->effectshader?(unsigned)(p->effectshader-cg_atmFx.effectshaders)+1u:0u;
        /* Exclude the process address of effectshader, retaining its index. */
        for(j=0;j<offsetof(cg_atmosphericParticle_t,effectshader);++j) hash=(hash^bytes[j])*16777619u;
        hash=(hash^shader)*16777619u;
    }
    return hash;
}
static void benchmark(const char *name, int frames, int closed, const float *origin)
{
    int i, j; double start, elapsed;
    setup(10000, SNOW_DENSITY); cg_atmFx.ParticleGenerate = real_generate;
    cg_atmFx.ParticleCheckVisible = CG_SnowParticleCheckVisible;
    memcpy(cg.refdef.vieworg, origin, sizeof(vec3_t));
    if (closed) for (i=0; i<256; ++i) for (j=0; j<256; ++j) tracemap.sky[i][j] = MAX_WORLD_HEIGHT;
    srand(173);
    start = seconds();
    for (i=0; i<frames; ++i) { cg.time = 10000+i*16; CG_AddAtmosphericEffects(); }
    elapsed = seconds()-start;
    printf("BENCH %s frames=%d ms_per_frame=%.6f generates=%d failed=%d renders=%d active=%d state=%08x next_rand=%d\n",
           name, frames, elapsed*1000/frames, generates, generationFailures, renders, cg_atmFx.dropsActive,particle_state_hash(),rand());
}
int main(int argc, char **argv)
{
    int i, first, second, beforeRand, afterRand;
    cg_atmosphericParticle_t expected;
    vec3_t vector = {50, 50, -80};
    char map[] = "obj_snow"; FILE *file;
    if (argc != 2 || !(file=fopen(argv[1], "rb"))) return 2;
    fseek(file,0,SEEK_END); traceSize=ftell(file); rewind(file); traceData=malloc(traceSize);
    if (!traceData || fread(traceData,1,traceSize,file)!=traceSize) return 3; fclose(file);
    CHECK(BG_LoadTraceMap(map, snowMapMins, snowMapMaxs));
    setup(10000,1); CG_AddAtmosphericEffects(); CHECK(generates==1);
    for (i=1;i<63;++i) { cg.time=10000+i*16; CG_AddAtmosphericEffects(); }
    CHECK(generates==1); CHECK(cg_atmFx.particles[0].nextDropTime==11000);
    cg.time=11000; CG_AddAtmosphericEffects(); CHECK(generates==2);
    cg.time=11001; CG_AddAtmosphericEffects(); CHECK(generates==2);

    setup(50000,4000); srand(117); init_deadlines(); first=INT_MAX; second=INT_MIN;
    for(i=0;i<4000;++i) { int delay=TCE_SubInt32(cg_atmFx.particles[i].nextDropTime,cg.time);
        CHECK(delay>=1000&&delay<2000); if(delay<first)first=delay;if(delay>second)second=delay; }
    CHECK(first<second); CG_AddAtmosphericEffects(); CHECK(generates==0);
    cg.time=51000; CG_AddAtmosphericEffects(); CHECK(generates>0&&generates<4000);
    cg.time=52000; CG_AddAtmosphericEffects(); CHECK(generates>=4000);

    setup(INT_MAX-500,1); CG_AddAtmosphericEffects(); CHECK(generates==1);
    cg.time=TCE_AddInt32(INT_MAX-500,999); CG_AddAtmosphericEffects(); CHECK(generates==1);
    cg.time=TCE_AddInt32(INT_MAX-500,1000); CG_AddAtmosphericEffects(); CHECK(generates==2);
    setup(-1000,1); cg_atmFx.particles[0].nextDropTime=-1000;
    CG_AddAtmosphericEffects(); CHECK(generates==1&&cg_atmFx.particles[0].nextDropTime==0);
    cg.time=-1;CG_AddAtmosphericEffects();CHECK(generates==1);
    cg.time=0;CG_AddAtmosphericEffects();CHECK(generates==2);
    setup(INT_MIN+500,1); cg_atmFx.particles[0].nextDropTime=1000;
    CG_AddAtmosphericEffects(); CHECK(generates==1);
    setup(5000,1); CG_AddAtmosphericEffects(); cg.time=50; CG_AddAtmosphericEffects(); CHECK(generates==2);
    cg.time=51; CG_AddAtmosphericEffects(); CHECK(generates==2);

    setup(2000,1); forceFailure=0; CG_AddAtmosphericEffects(); CHECK(generates==1&&renders==1);
    cg_atmFx.particles[0].nextDropTime=999999;
    for(i=1;i<=10;++i) {cg.time=2000+i*16;CG_AddAtmosphericEffects();}
    CHECK(generates==1&&renders==11&&visibilityChecks==11);
    CHECK(cg_atmFx.particles[0].pos[0]==10);

    /* An immediately due successful real spawn consumes the identical RNG and
     * produces the same particle data as the real generator called directly. */
    setup(10000,1); tracemap.loaded=qtrue;
    for(i=0;i<256*256;++i) { ((float*)tracemap.sky)[i]=1000; ((float*)tracemap.skyground)[i]=0; }
    cg_atmFx.ParticleGenerate=real_generate; memset(&expected,0,sizeof(expected));
    srand(23); CHECK(CG_SnowParticleGenerate(&expected,vector,1)); beforeRand=rand();
    srand(23); CG_AddAtmosphericEffects(); afterRand=rand();
    CHECK(beforeRand==afterRand); CHECK(!memcmp(&expected,&cg_atmFx.particles[0],sizeof(expected)));
    setup(1000,0); CG_AddAtmosphericEffects(); CHECK(!generates&&!renders);
    setup(1000,1); cg_atmFx.numEffectShaders=0; CG_AddAtmosphericEffects(); CHECK(!generates&&!renders);

    setup(10000,SNOW_DENSITY); for(i=0;i<63;++i) {cg.time=10000+i*16;CG_AddAtmosphericEffects();}
    CHECK(generates==SNOW_DENSITY);
    printf("INITIAL_STAGGER min=%d max=%d\n",first,second);
    CHECK(BG_LoadTraceMap(map,snowMapMins,snowMapMaxs));
    benchmark("snow_real_tracemap_spawn",600,0,snowSpawn);
    benchmark("snow_failed_sky_control",600,1,snowSpawn);
    printf("%d actual weather retry checks, %d failures\n",checks,failures);
    free(traceData); return failures!=0;
}
