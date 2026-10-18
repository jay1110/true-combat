"""Run production spawn admission/selectors against independent native geometry.

The real g_local.h supplies entities, clients, traces and content constants.
Only engine queries, entity lookup, token parsing and randomness are fixtures.
Static world boxes and live-player boxes are independent; the production
G_SpawnOriginWorldClear and SpotWouldTelefrag bodies always run unchanged.
This is an offline regression, not a BSP collision or live-map certificate.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT.parent / 'build'


def body(source, marker):
    begin = source.index(marker)
    opening = source.index('{', begin)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[begin:end]


def production_program():
    client = (ROOT / 'game/g_client.c').read_text()
    team = (ROOT / 'game/g_team.c').read_text()
    game = (ROOT / 'game/g_main.c').read_text()
    functions = '\n'.join(body(client, marker) for marker in (
        'qboolean SpotWouldTelefrag(',
        'qboolean G_SpawnOriginWorldClear(',
        'gentity_t *SelectNearestDeathmatchSpawnPoint(',
        'gentity_t *SelectRandomDeathmatchSpawnPoint(',
        'gentity_t *SelectSpawnPoint (',
        'gentity_t *SelectSpawnPointFromList(',
        'gentity_t *SelectIntermissionSpawnPoint(',
        'gentity_t *SelectSpectatorSpawnPoint('))
    functions += '\n' + body(game, 'void FindIntermissionPoint( void )')
    functions += '\n' + '\n'.join(body(team, marker) for marker in (
        'static double TCE_SpawnDistance(',
        'gentity_t *SelectRandomTeamSpawnPoint(',
        'gentity_t *SelectCTFSpawnPoint ('))
    header = (ROOT / 'game/g_local.h').as_posix()
    production = ('#include "' + header + '"\n'
                  'extern int fixtureRand(void);\n#define rand fixtureRand\n'
                  '#define MAX_SPAWN_POINTS 128\n#define MAX_TEAM_SPAWN_POINTS 256\n'
                  + functions)
    return SHIM.replace('@GAME_HEADER@', header) + CASES, production


def compile_run(test, programs):
    BUILD.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='tce-spawn-unit-', dir=BUILD) as folder:
        directory = Path(folder)
        (directory / 'main.cpp').write_text(programs[0], encoding='utf-8')
        (directory / 'production.c').write_text(programs[1], encoding='utf-8')
        (directory / 'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.15)\n'
            'project(SpawnAdmissionTests LANGUAGES C CXX)\n'
            'set(CMAKE_CXX_STANDARD 17)\n'
            'add_executable(spawn-test main.cpp production.c)\n'
            'target_compile_definitions(spawn-test PRIVATE GAMEDLL)\n')
        options = dict(capture_output=True, text=True, timeout=90,
                       creationflags=subprocess.CREATE_NO_WINDOW)
        for command in (
            ['cmake', '-S', folder, '-B', str(directory / 'build'), '-A', 'Win32'],
            ['cmake', '--build', str(directory / 'build'), '--config', 'Release'],
            [str(directory / 'build/Release/spawn-test.exe')],
        ):
            result = subprocess.run(command, **options)
            test.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        test.assertIn('PASS 9 native spawn admission fixture groups', result.stdout)


SHIM = r'''
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <csetjmp>
#include <vector>
extern "C" {
#include "@GAME_HEADER@"
gentity_t *SelectNearestDeathmatchSpawnPoint(vec3_t);
gentity_t *SelectRandomDeathmatchSpawnPoint(void);
gentity_t *SelectRandomTeamSpawnPoint(int,team_t,int,int,qboolean);
gentity_t *SelectCTFSpawnPoint(team_t,int,vec3_t,vec3_t,int,int,qboolean);
gentity_t *SelectSpawnPointFromList(char *,vec3_t,vec3_t);
gentity_t *SelectSpectatorSpawnPoint(vec3_t,vec3_t);
}
#undef assert
#define assert(condition) do { if (!(condition)) { \
    fprintf(stderr,"assertion at line %d: %s\n",__LINE__,#condition); exit(1); \
} } while (0)

gentity_t g_entities[MAX_GENTITIES];
level_locals_t level;
vmCvar_t g_newbbox, g_gametype;
vec3_t vec3_origin = {0, 0, 0};
static gclient_t fixtureClients[8];
static int randomValue, randomCalls, hitStyle;
static jmp_buf fatalJump;
static bool fatalExpected;
static char fatalText[256];
extern "C" int fixtureRand() { ++randomCalls; return randomValue; }
extern "C" void trap_GetConfigstring(int, char *text, int size) {
    assert(size > 2); std::strcpy(text,"0");
}
extern "C" char *Info_ValueForKey(const char *, const char *) { return const_cast<char *>("0"); }
extern "C" gentity_t *G_PickTarget(char *) { return nullptr; }
extern "C" void vectoangles(const vec3_t, vec3_t angles) { VectorClear(angles); }

struct Box { vec3_t lo, hi; int contents, entity; };
struct TraceCall { vec3_t start, end, mins, maxs; int pass, mask; };
static std::vector<Box> world, players;
static std::vector<TraceCall> traceCalls;

// Euclidean distance between a vertical capsule axis and an axis-aligned box.
// This collision oracle depends only on the supplied hull and geometry.
static bool capsuleOverlaps(const TraceCall &call, const Box &box) {
    const float radius = (call.maxs[0] - call.mins[0]) * .5f;
    const float centerX = call.start[0] + (call.mins[0] + call.maxs[0]) * .5f;
    const float centerY = call.start[1] + (call.mins[1] + call.maxs[1]) * .5f;
    const float low = call.start[2] + call.mins[2] + radius;
    const float high = call.start[2] + call.maxs[2] - radius;
    const float dx = centerX < box.lo[0] ? box.lo[0] - centerX :
        centerX > box.hi[0] ? centerX - box.hi[0] : 0;
    const float dy = centerY < box.lo[1] ? box.lo[1] - centerY :
        centerY > box.hi[1] ? centerY - box.hi[1] : 0;
    const float dz = high < box.lo[2] ? box.lo[2] - high :
        low > box.hi[2] ? low - box.hi[2] : 0;
    return dx * dx + dy * dy + dz * dz < radius * radius;
}

void trap_TraceCapsule(trace_t *out, const vec3_t start, const vec3_t mins,
                      const vec3_t maxs, const vec3_t end, int pass, int mask) {
    TraceCall call;
    VectorCopy(start, call.start); VectorCopy(end, call.end);
    VectorCopy(mins, call.mins); VectorCopy(maxs, call.maxs);
    call.pass = pass; call.mask = mask; traceCalls.push_back(call);
    assert(!memcmp(start, end, sizeof(vec3_t)));
    memset(out, 0, sizeof(*out)); out->fraction = 1;
    for (const auto &box : world) {
        if ((box.contents & mask) && capsuleOverlaps(call, box)) {
            // Exercise each native trace rejection signal on a geometric hit.
            out->startsolid = hitStyle == 0 || hitStyle == 1 ? qtrue : qfalse;
            out->allsolid = hitStyle == 0 || hitStyle == 2 ? qtrue : qfalse;
            out->fraction = hitStyle == 0 ? 0 : hitStyle == 3 ? .5f : 1;
            out->entityNum = ENTITYNUM_WORLD; return;
        }
    }
}

int trap_EntitiesInBox(const vec3_t mins, const vec3_t maxs, int *list, int limit) {
    int count = 0;
    for (const auto &box : players) {
        bool overlap = true;
        for (int axis = 0; axis < 3; ++axis)
            if (maxs[axis] < box.lo[axis] || mins[axis] > box.hi[axis]) overlap = false;
        if (overlap && count < limit) list[count++] = box.entity;
    }
    return count;
}

gentity_t *G_Find(gentity_t *from, int offset, const char *name) {
    assert(offset == FOFS(classname));
    int first = from ? int(from - g_entities) + 1 : 0;
    for (int i = first; i < level.num_entities; ++i)
        if (g_entities[i].classname && !strcmp(g_entities[i].classname, name))
            return &g_entities[i];
    return NULL;
}
gentity_t *G_FindByTargetname(gentity_t *from, const char *name) {
    int first = from ? int(from - g_entities) + 1 : 0;
    for (int i = first; i < level.num_entities; ++i)
        if (g_entities[i].targetname && !strcmp(g_entities[i].targetname, name))
            return &g_entities[i];
    return NULL;
}
char *COM_Parse(char **input) {
    static char token[128]; int count = 0;
    if (!*input) { token[0] = 0; return token; }
    while (**input && isspace((unsigned char)**input)) ++*input;
    while (**input && !isspace((unsigned char)**input)) {
        assert(count < 127); token[count++] = *(*input)++;
    }
    token[count] = 0; return token;
}
void QDECL G_Error(const char *format, ...) {
    assert(strlen(format) < sizeof(fatalText)); strcpy(fatalText,format);
    if (!fatalExpected) { fprintf(stderr,"unexpected engine error: %s\n",format); exit(2); }
    longjmp(fatalJump,1);
}
vec_t VectorLength(const vec3_t vector) {
    return sqrtf(vector[0]*vector[0] + vector[1]*vector[1] + vector[2]*vector[2]);
}

static void reset(int profile = 0) {
    memset(g_entities, 0, sizeof(g_entities)); memset(&level, 0, sizeof(level));
    memset(fixtureClients, 0, sizeof(fixtureClients));
    world.clear(); players.clear(); traceCalls.clear();
    g_newbbox.integer = profile; g_gametype.integer = 2;
    randomValue = randomCalls = hitStyle = 0;
    fatalExpected = false; fatalText[0] = 0;
    level.maxclients = 8; level.num_entities = 64; level.numspawntargets = 2;
    level.axisAutoSpawn = 0; level.alliesAutoSpawn = 1;
    VectorSet(level.spawntargets[1], 1000, 0, 0);
}
static gentity_t *spawn(int index, const char *name, float x,
                        const char *target = "entry") {
    gentity_t *spot = &g_entities[index];
    spot->s.number = index; spot->classname = const_cast<char *>(name);
    spot->targetname = const_cast<char *>(target); spot->spawnflags = 2;
    VectorSet(spot->s.origin, x, 0, 0); VectorCopy(spot->s.origin, spot->r.currentOrigin);
    VectorSet(spot->s.angles, index, index + 1, index + 2);
    return spot;
}
static void box(float x1, float y1, float z1, float x2, float y2, float z2,
                int contents = CONTENTS_SOLID) {
    Box value = {{x1,y1,z1},{x2,y2,z2},contents,ENTITYNUM_WORLD}; world.push_back(value);
}
static void block(float x) { box(x-2,-2,-100,x+2,2,100); }
static void occupy(float x, int client = 0, int health = 100) {
    Box value = {{x-1,-1,-1},{x+1,1,1},CONTENTS_BODY,client}; players.push_back(value);
    g_entities[client].client = &fixtureClients[client];
    fixtureClients[client].ps.stats[STAT_HEALTH] = health;
}
static void coordinates(const gentity_t *spot, const vec3_t origin,
                        const vec3_t angles, bool team) {
    const float *base = team ? spot->s.origin : spot->r.currentOrigin;
    assert(origin[0] == base[0] && origin[1] == base[1] && origin[2] == base[2]+9);
    assert(!memcmp(angles, spot->s.angles, sizeof(vec3_t)));
}
'''


CASES = r'''
static void helperProfilesAndSignals() {
    for (int profile = 0; profile < 2; ++profile) {
        reset(profile); vec3_t base = {100,200,300}, before;
        VectorCopy(base, before); assert(G_SpawnOriginWorldClear(base));
        assert(!memcmp(base, before, sizeof(base)) && traceCalls.size() == 1);
        const TraceCall &call = traceCalls.back();
        assert(call.start[0] == 100 && call.start[1] == 200 && call.start[2] == 309);
        assert(call.mins[0] == (profile ? -16 : -14) && call.mins[1] == call.mins[0]);
        assert(call.maxs[0] == -call.mins[0] && call.maxs[1] == call.maxs[0]);
        assert(call.mins[2] == (profile ? -30 : -24));
        assert(call.maxs[2] == (profile ? 57 : 46));
        assert(call.maxs[2]-call.mins[2] == (profile ? 87 : 70));
        assert(call.pass == ENTITYNUM_NONE);
        assert(call.mask == (CONTENTS_SOLID | CONTENTS_PLAYERCLIP));
        box(99,199,299,101,201,301,CONTENTS_BODY | CONTENTS_CORPSE | CONTENTS_TRIGGER);
        assert(G_SpawnOriginWorldClear(base));
        for (int contents : {CONTENTS_SOLID, CONTENTS_PLAYERCLIP}) {
            world.clear(); box(99,199,299,101,201,301,contents);
            for (hitStyle = 0; hitStyle < 4; ++hitStyle)
                assert(!G_SpawnOriginWorldClear(base));
        }
    }
}

static void exactPoseAndCapsuleGeometry() {
    vec3_t base = {0,0,0};
    reset(); box(15,-100,-100,15.5f,100,100);
    assert(G_SpawnOriginWorldClear(base)); g_newbbox.integer = 1;
    assert(!G_SpawnOriginWorldClear(base)); // 14 versus 16 half-width
    reset(); box(-100,-100,56,100,100,57);
    assert(G_SpawnOriginWorldClear(base)); g_newbbox.integer = 1;
    assert(!G_SpawnOriginWorldClear(base)); // 70 versus 87 standing height
    reset(); box(-100,-100,-20,100,100,-19);
    assert(G_SpawnOriginWorldClear(base)); g_newbbox.integer = 1;
    assert(!G_SpawnOriginWorldClear(base)); // 24 versus 30 bottom offset
    for (int profile = 0; profile < 2; ++profile) {
        reset(profile); const float top = profile ? 57.f : 46.f;
        box(-100,-100,top+7,100,100,top+8);
        assert(!G_SpawnOriginWorldClear(base)); // requires unchanged +9, not base pose
        world.clear(); const float bottom = profile ? -30.f : -24.f;
        box(-100,-100,bottom+1,100,100,bottom+2);
        assert(G_SpawnOriginWorldClear(base)); // +9 has lifted the body above this slab
        world.clear(); box(13,13,0,13.5f,13.5f,1);
        assert(G_SpawnOriginWorldClear(base)); // outside rounded capsule, inside AABB
    }
}

static void independentPlayerOverlap() {
    for (int profile = 0; profile < 2; ++profile) {
        reset(profile); gentity_t *spot = spawn(10,"info_player_deathmatch",0);
        occupy(0); box(-2,-2,-2,2,2,2,CONTENTS_BODY);
        assert(SpotWouldTelefrag(spot) && G_SpawnOriginWorldClear(spot->r.currentOrigin));
        fixtureClients[0].ps.stats[STAT_HEALTH] = 0;
        assert(!SpotWouldTelefrag(spot)); block(0);
        assert(!SpotWouldTelefrag(spot) && !G_SpawnOriginWorldClear(spot->r.currentOrigin));
    }
}

static void deathmatchAndStableRandomChoice() {
    for (int profile = 0; profile < 2; ++profile) {
        reset(profile);
        gentity_t *first = spawn(11,"info_player_deathmatch",100);
        gentity_t *second = spawn(12,"info_player_deathmatch",200);
        randomValue = 1; assert(SelectRandomDeathmatchSpawnPoint() == second && randomCalls == 1);
        gentity_t *blocked = spawn(10,"info_player_deathmatch",0); block(0);
        assert(SelectRandomDeathmatchSpawnPoint() == second); // same valid sequence/random value
        randomValue = 0; assert(SelectRandomDeathmatchSpawnPoint() == first);
        occupy(100); assert(SelectRandomDeathmatchSpawnPoint() == second);
        occupy(200,1); assert(SelectRandomDeathmatchSpawnPoint() == first); // all occupied fallback
        VectorSet(blocked->s.origin,1000,0,0); // DM admission follows r.currentOrigin
        VectorSet(first->s.origin,0,0,0); VectorSet(second->s.origin,0,0,0);
        vec3_t avoid = {-1000,0,0}, origin, angles;
        assert(SelectSpawnPoint(avoid,origin,angles) == first); coordinates(first,origin,angles,false);
        block(100); block(200); assert(!SelectRandomDeathmatchSpawnPoint());
        fatalExpected = true;
        if (!setjmp(fatalJump)) { SelectSpawnPoint(avoid,origin,angles); assert(false); }
        assert(!strcmp(fatalText,"Couldn't find a spawn point")); fatalExpected = false;
    }
}

static void teamPreferredPhaseHostageAndOrigins() {
    for (int profile = 0; profile < 2; ++profile) {
        reset(profile);
        gentity_t *blocked = spawn(10,"team_CTF_redspawn",0); block(0);
        gentity_t *near = spawn(11,"team_CTF_redspawn",100);
        gentity_t *far = spawn(12,"team_CTF_redspawn",500);
        assert(SelectRandomTeamSpawnPoint(1,TEAM_AXIS,1,10,qfalse) == near);
        assert(SelectRandomTeamSpawnPoint(1,TEAM_AXIS,1,12,qfalse) == far);
        occupy(500); assert(SelectRandomTeamSpawnPoint(1,TEAM_AXIS,1,12,qfalse) == near);
        players.clear(); near->tceObjectiveScore = 0; far->tceObjectiveScore = 2;
        level.tceSpawnPhase = 2;
        assert(SelectRandomTeamSpawnPoint(1,TEAM_AXIS,1,0,qfalse) == far);
        VectorSet(near->r.currentOrigin,0,0,0); // team admission/wrapper follows s.origin
        VectorSet(far->r.currentOrigin,0,0,0);
        VectorSet(blocked->r.currentOrigin,1000,0,0);
        vec3_t origin, angles;
        assert(SelectCTFSpawnPoint(TEAM_AXIS,1,origin,angles,1,12,qfalse) == far);
        coordinates(far,origin,angles,true);
        spawn(13,"team_CTF_bluespawn",900); spawn(14,"team_CTF_bluespawn",1100);
        assert(SelectRandomTeamSpawnPoint(1,TEAM_ALLIES,0,0,qfalse) == &g_entities[13]);
        gentity_t *hostage = spawn(15,"team_hostage_spawn",700);
        g_gametype.integer = 7; level.tceCustomDMSpawns = qtrue;
        spawn(16,"info_player_deathmatch",800);
        assert(SelectRandomTeamSpawnPoint(1,TEAM_AXIS,1,999,qtrue) == hostage);
        level.tceCustomDMSpawns = qfalse;
        assert(!SelectRandomTeamSpawnPoint(1,TEAM_SPECTATOR,1,0,qfalse));
    }
}

static void bothTeamOccupiedFallbackPasses() {
    for (int profile = 0; profile < 2; ++profile) {
        reset(profile);
        spawn(10,"team_CTF_redspawn",0); block(0);
        gentity_t *disabled = spawn(11,"team_CTF_redspawn",100); disabled->spawnflags = 0;
        gentity_t *occupied = spawn(12,"team_CTF_redspawn",200); occupy(200);
        // First !count fallback skips blocked and disabled spots, retains occupied enabled spot.
        assert(SelectRandomTeamSpawnPoint(1,TEAM_AXIS,1,0,qfalse) == occupied);
        occupied->entstate = STATE_INVISIBLE;
        // Second !count fallback retains the original any-state policy, still world-clear.
        assert(SelectRandomTeamSpawnPoint(1,TEAM_AXIS,1,0,qfalse) == disabled);
        block(100); assert(SelectRandomTeamSpawnPoint(1,TEAM_AXIS,1,0,qfalse) == occupied);
        block(200); assert(!SelectRandomTeamSpawnPoint(1,TEAM_AXIS,1,0,qfalse));
    }
}

static void customDeathmatchAndTeamWrapperFallback() {
    for (int profile = 0; profile < 2; ++profile) {
        reset(profile); g_gametype.integer = 7; level.tceCustomDMSpawns = qtrue;
        gentity_t *normal = spawn(20,"team_CTF_redspawn",500);
        spawn(10,"info_player_deathmatch",0); block(0);
        gentity_t *first = spawn(11,"info_player_deathmatch",100);
        gentity_t *second = spawn(12,"info_player_deathmatch",200);
        randomValue = 1; assert(SelectRandomTeamSpawnPoint(1,TEAM_AXIS,1,0,qfalse) == second);
        randomValue = 0; assert(SelectRandomTeamSpawnPoint(1,TEAM_AXIS,1,0,qfalse) == first);
        VectorSet(first->r.currentOrigin,0,0,0); // custom team path uses s.origin, unlike DM
        assert(SelectRandomTeamSpawnPoint(1,TEAM_AXIS,1,0,qfalse) == first);
        first->entstate = STATE_INVISIBLE; occupy(200);
        assert(SelectRandomTeamSpawnPoint(1,TEAM_AXIS,1,0,qfalse) == normal);
        level.tceCustomDMSpawns = qfalse; block(500);
        vec3_t origin, angles;
        assert(SelectCTFSpawnPoint(TEAM_AXIS,1,origin,angles,1,0,qfalse) == second);
        coordinates(second,origin,angles,false); // occupied world-clear DM fallback
        block(200); assert(!SelectRandomTeamSpawnPoint(1,TEAM_AXIS,1,0,qfalse));
        fatalExpected = true;
        if (!setjmp(fatalJump)) {
            SelectCTFSpawnPoint(TEAM_AXIS,1,origin,angles,1,0,qfalse); assert(false);
        }
        assert(!strcmp(fatalText,"Couldn't find a spawn point")); fatalExpected = false;
        reset(profile); spawn(10,"team_CTF_redspawn",100); level.numspawntargets = 0;
        fatalExpected = true;
        if (!setjmp(fatalJump)) {
            SelectRandomTeamSpawnPoint(1,TEAM_AXIS,1,0,qfalse); assert(false);
        }
        assert(!strcmp(fatalText,"No spawnpoints found\n")); fatalExpected = false;
        // The unrelated original objective error remains unchanged.
    }
}

static void listSelectionFallbackAndCoordinates() {
    for (int profile = 0; profile < 2; ++profile) {
        reset(profile);
        spawn(10,"list_spawn",0,"entry"); block(0);
        gentity_t *first = spawn(11,"list_spawn",100,"entry");
        gentity_t *second = spawn(12,"list_spawn",200,"other");
        VectorSet(first->s.origin,0,0,0); VectorSet(second->s.origin,0,0,0);
        vec3_t origin = {-11,-12,-13}, angles = {-21,-22,-23};
        char list[] = "entry other"; randomValue = 1;
        assert(SelectSpawnPointFromList(list,origin,angles) == second);
        coordinates(second,origin,angles,false);
        randomValue = 0; assert(SelectSpawnPointFromList(list,origin,angles) == first);
        occupy(100); assert(SelectSpawnPointFromList(list,origin,angles) == second);
        occupy(200,1); assert(SelectSpawnPointFromList(list,origin,angles) == first);
        coordinates(first,origin,angles,false); // occupied fallback must also write the pose
        block(100); assert(SelectSpawnPointFromList(list,origin,angles) == second);
        coordinates(second,origin,angles,false);
        block(200); VectorSet(origin,-11,-12,-13); VectorSet(angles,-21,-22,-23);
        assert(!SelectSpawnPointFromList(list,origin,angles));
        assert(origin[0] == -11 && origin[1] == -12 && origin[2] == -13);
        assert(angles[0] == -21 && angles[1] == -22 && angles[2] == -23);
        char missing[] = "missing"; assert(!SelectSpawnPointFromList(missing,origin,angles));
    }
}

static void spectatorCameraIsNotAPlayingBody() {
    for (int profile=0;profile<2;++profile) {
        reset(profile);vec3_t origin,angles;
        gentity_t *spot=spawn(10,"info_player_deathmatch",0);block(0);
        assert(!SelectRandomDeathmatchSpawnPoint());traceCalls.clear();
        assert(!SelectSpectatorSpawnPoint(origin,angles));
        coordinates(spot,origin,angles,false);
        assert(traceCalls.empty()); // Camera uses the authored pose without a body proof.
        fatalExpected=true;
        if(!setjmp(fatalJump)){SelectSpawnPoint(vec3_origin,origin,angles);assert(false);}
        assert(!strcmp(fatalText,"Couldn't find a spawn point"));fatalExpected=false;
        gentity_t *playing=spawn(11,"team_CTF_redspawn",256);
        assert(SelectCTFSpawnPoint(TEAM_AXIS,1,origin,angles,1,0,qfalse)==playing);
        coordinates(playing,origin,angles,true);
        gentity_t *camera=spawn(12,"info_player_intermission",512);camera->spawnflags=0;
        traceCalls.clear();assert(!SelectSpectatorSpawnPoint(origin,angles));
        assert(!memcmp(origin,camera->s.origin,sizeof(vec3_t)));
        assert(!memcmp(angles,camera->s.angles,sizeof(vec3_t))&&traceCalls.empty());
    }
}

int main() {
    helperProfilesAndSignals(); exactPoseAndCapsuleGeometry(); independentPlayerOverlap();
    deathmatchAndStableRandomChoice(); teamPreferredPhaseHostageAndOrigins();
    bothTeamOccupiedFallbackPasses(); customDeathmatchAndTeamWrapperFallback();
    listSelectionFallbackAndCoordinates();
    spectatorCameraIsNotAPlayingBody();
    printf("PASS 9 native spawn admission fixture groups (both standing profiles)\n");
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class NativeSpawnWorldAdmissionTests(TestCase):
    def test_real_helper_and_all_spawn_selectors(self):
        compile_run(self, production_program())


if __name__ == '__main__':
    main()
