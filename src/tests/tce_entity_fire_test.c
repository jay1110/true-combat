#include "../cgame/cg_local.h"
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>

cg_t cg;
static snapshot_t snapshot;
static int cases;
static void require(int ok, const char *message) {
    if (!ok) { fprintf(stderr, "Fire case %d: %s\n", cases, message); exit(1); }
}
static void compare(unsigned char *base, int local, int time, int start, int end, int snapStart) {
    centity_t cent, saved;
    unsigned char raw[0x200], before[0x200], rawSnapshot[0x500], savedSnapshot[0x500];
    snapshot_t savedSdkSnapshot;
    int expected, actual;
    int (__cdecl *original)(void *) = (void *)(base + 0x52cd0);
    memset(&cent, 0x5a, sizeof(cent)); memset(raw, 0x5a, sizeof(raw));
    memset(&snapshot, 0x3c, sizeof(snapshot)); memset(rawSnapshot, 0x3c, sizeof(rawSnapshot));
    cent.currentState.number = *(int *)raw = local ? 7 : 9;
    cent.currentState.onFireStart = *(int *)(raw + 0xfc) = start;
    cent.currentState.onFireEnd = *(int *)(raw + 0x100) = end;
    snapshot.ps.clientNum = *(int *)(rawSnapshot + 0xcc) = 7;
    snapshot.ps.onFireStart = *(int *)(rawSnapshot + 0x450) = snapStart;
    cg.snap = &snapshot; cg.time = *(int *)(base + 0x407dea4) = time;
    *(void **)(base + 0x40352c4) = rawSnapshot;
    saved = cent; savedSdkSnapshot = snapshot;
    memcpy(before, raw, sizeof(raw)); memcpy(savedSnapshot, rawSnapshot, sizeof(rawSnapshot));
    expected = original(raw); actual = CG_EntOnFire(&cent);
    require(actual == expected, "original return mismatch");
    require(actual == (local ? (snapStart != 0 && snapStart < time && time < snapStart + 2000)
                            : (start < time && time < end)), "exclusive interval semantics");
    require(!memcmp(&cent, &saved, sizeof(cent)) && !memcmp(raw, before, sizeof(raw)), "entity changed");
    require(!memcmp(&snapshot, &savedSdkSnapshot, sizeof(snapshot)) &&
            !memcmp(rawSnapshot, savedSnapshot, sizeof(rawSnapshot)), "snapshot changed");
    require(cg.time == time && *(int *)(base + 0x407dea4) == time, "time changed");
    ++cases;
}
int main(int argc, char **argv) {
    const int starts[] = {-100000, -2000, -1, 0, 1, 999, 100000, 2000000000};
    const int offsets[] = {-2001, -1, 0, 1, 1499, 1500, 1999, 2000, 2001, 5000};
    int a,b,c,d,e; HMODULE dll; unsigned char *base; void *oldSnapshot; int oldTime;
    require(argc == 2, "DLL argument"); dll = LoadLibraryA(argv[1]); require(dll != NULL, "load original");
    base = (unsigned char *)dll; oldSnapshot = *(void **)(base + 0x40352c4); oldTime = *(int *)(base + 0x407dea4);
    for(a=0;a<2;++a) for(b=0;b<8;++b) for(c=0;c<10;++c) for(d=0;d<10;++d) for(e=0;e<8;++e)
        compare(base,a,starts[b]+offsets[c],starts[b],starts[b]+offsets[d],starts[e]);
    *(void **)(base + 0x40352c4) = oldSnapshot; *(int *)(base + 0x407dea4) = oldTime;
    FreeLibrary(dll); printf("%d entity-fire original comparisons passed.\n", cases); return 0;
}
