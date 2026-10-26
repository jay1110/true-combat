#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include <limits.h>
#include "../game/q_shared.h"
#include "../game/bg_public.h"

int numSplinePaths;
splinePath_t splinePaths[MAX_SPLINE_PATHS];
static int cases;
static void require(int ok, const char *why) {
    if (!ok) { fprintf(stderr, "case %d: %s\n", cases, why); exit(1); }
}
static void compare(unsigned char *base, int number, int initial) {
    struct { unsigned before; qboolean value; unsigned after; } actual, expected;
    splinePath_t *result, *reference;
    splinePath_t *(__cdecl *original)(int, qboolean *) = (void *)(base + 0x5c30);
    int *originalCount = (int *)(base + 0x4bab708);
    int savedCount = *originalCount;
    actual.before = 0xabcdef01; actual.after = 0x12345678;
    actual.value = initial; expected = actual;
    *originalCount = numSplinePaths;
    reference = original(number, &expected.value);
    require(*originalCount == numSplinePaths, "original count mutated");
    *originalCount = savedCount;
    result = BG_GetSplineData(number, &actual.value);
    require(!memcmp(&actual, &expected, sizeof(actual)), "direction or guard mismatch");
    require((result == NULL) == (reference == NULL), "presence mismatch");
    if (result) {
        require((unsigned char *)result - (unsigned char *)splinePaths ==
            (unsigned char *)reference - (base + 0x4bab720), "path offset mismatch");
    }
    ++cases;
}
int main(int argc, char **argv) {
    HMODULE dll; unsigned char *base; int n, id, initial;
    require(sizeof(splinePath_t) == 0x394, "original spline stride");
    require(argc == 2, "original DLL path");
    dll = LoadLibraryA(argv[1]); require(dll != NULL, "load original DLL");
    base = (unsigned char *)dll;
    for (n = -1; n <= MAX_SPLINE_PATHS; ++n) {
        numSplinePaths = n;
        for (initial = -1; initial <= 1; ++initial) {
            for (id = -MAX_SPLINE_PATHS-2; id <= MAX_SPLINE_PATHS+2; ++id)
                compare(base, id, initial);
            compare(base, INT_MIN, initial);
            compare(base, INT_MIN+1, initial);
            compare(base, INT_MAX, initial);
        }
        require(numSplinePaths == n, "port count mutated");
    }
    FreeLibrary(dll);
    printf("%d exact BG_GetSplineData original comparisons passed.\n", cases);
    return 0;
}
