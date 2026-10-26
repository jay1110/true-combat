#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../cgame/tce_weapon_recoil.h"
static int value,calls;
int rand(void) { return calls++ ? 32767-value : value; }
int main(int argc,char **argv) {
    HMODULE dll;unsigned char *base,*p,saved[5];DWORD old,tmp;
    float expected[3],actual[3];int w,v,n=0,expectedCalls;
    void (__cdecl *original)(int);
    if(argc!=2 || !(dll=LoadLibraryA(argv[1]))) return 2;
    base=(unsigned char *)dll;original=(void *)(base+0x75a00);p=base+0x885b6;
    memcpy(saved,p,5);if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old)) return 2;
    p[0]=0xe9;*(int *)(p+1)=(unsigned char *)rand-p-5;
    FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);
    for(w=-2;w<=68;++w)for(v=0;v<32768;++v) {
        actual[0]=3.25f;actual[1]=-6.125f;actual[2]=9.75f;
        memcpy(base+0x40810ec,actual,12);value=v;calls=0;original(w);
        memcpy(expected,base+0x40810ec,12);expectedCalls=calls;
        value=v;calls=0;TCE_CG_WeaponFireRecoil(w,actual);
        if(calls!=expectedCalls||memcmp(expected,actual,12)) {
            fprintf(stderr,"weapon%d RNG%d calls%d/%d result %.9g %.9g %.9g expected %.9g %.9g %.9g\n",w,v,calls,expectedCalls,actual[0],actual[1],actual[2],expected[0],expected[1],expected[2]);return 1;
        }++n;
    }
    VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);memcpy(p,saved,5);
    FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);
    FreeLibrary(dll);printf("%d bit-exact original recoil/RNG comparisons passed.\n",n);return 0;
}
