#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_servercmds.c"
#include <windows.h>
#include <stdio.h>

int main(int argc, char **argv) {
    HMODULE dll;
    void (__cdecl *original)(char *);
    unsigned char a[1030], b[1030];
    unsigned int x, y, i, length, cases = 0;
    if (argc != 2 || !(dll = LoadLibraryA(argv[1]))) return 2;
    original = (void *)( (unsigned char *)dll + 0x5fc70 );
    for (x = 0; x < 256; ++x) for (y = 0; y < 256; ++y) {
        memset(a, 0xa5, sizeof(a)); a[1] = (unsigned char)x; a[2] = (unsigned char)y; a[3] = 0;
        memcpy(b, a, sizeof(a)); original((char *)a + 1); CG_RemoveChatEscapeChar((char *)b + 1);
        if (memcmp(a, b, sizeof(a))) { printf("pair %u %u mismatch\n", x, y); return 1; }
        ++cases;
    }
    for (length = 0; length <= 1024; ++length) for (x = 0; x < 4; ++x) {
        memset(a, 0xa5, sizeof(a));
        for (i = 0; i < length; ++i) a[i+1] = x == 0 ? 25 : x == 1 ? (i&1 ? 25 : '^') : x == 2 ? (unsigned char)(1+i%255) : 'A';
        a[length+1] = 0; memcpy(b, a, sizeof(a));
        original((char *)a + 1); CG_RemoveChatEscapeChar((char *)b + 1);
        if (memcmp(a, b, sizeof(a))) { printf("length %u pattern %u mismatch\n", length, x); return 1; }
        ++cases;
    }
    printf("%u whole chat escape comparisons passed, including buffer sentinels\n", cases);
    FreeLibrary(dll); return 0;
}
