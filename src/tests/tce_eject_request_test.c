#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../cgame/tce_eject_request.h"
int main(int argc,char **argv) {
    HMODULE dll;unsigned char *base,cent[0xb00],snap[0x1000];
    void (__cdecl *fire)(void *);int w,i,expected,actual,n=0;
    if(argc!=2 || !(dll=LoadLibraryA(argv[1]))) return 2;
    base=(unsigned char *)dll;fire=(void *)(base+0x766b0);
    memset(snap,0,sizeof(snap));*(int *)(snap+0xcc)=99;
    *(void **)(base+0x40352c4)=snap;
    *(int *)(base+0x40a4a1c)=0;
    /* Nonlocal events, no flash sounds: real dispatcher and weapon guards,
       including real BG_CheckUTWeapon; no original code hooks required. */
    for(w=1;w<64;++w)for(i=0;i<256;++i) {
        memset(cent,0,sizeof(cent));*(int *)cent=2;*(int *)(cent+0xac)=2;
        *(int *)(cent+0xe8)=w;*(int *)(cent+0xbc)=(i&1)?7:0;
        *(int *)(cent+0xa7c)=i%3-1;
        *(int *)(base+0x4c1df20+w*0x1cc+0x16c)=(i>>1)&1;
        *(int *)(base+0x4c1df20+w*0x1cc+0x170)=(i>>2)&1;
        *(int *)(base+0x484a080+w*0xfd0+0xfac)=(i>>3)&1;
        *(int *)(base+0x4b3292c)=(i>>4)%3-1;
        fire(cent);expected=*(int *)(cent+0xa7c);
        actual=TCE_CG_EjectRequest((i>>1)&1,(i>>2)&1,(i&1)?7:0,(i>>3)&1,(i>>4)%3-1,i%3-1);
        if(actual!=expected) {fprintf(stderr,"weapon%d variant%d: %d != %d\n",w,i,actual,expected);return 1;}
        ++n;
    }
    FreeLibrary(dll);printf("%d original fire-event ejection requests compared.\n",n);return 0;
}
