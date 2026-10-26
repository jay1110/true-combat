#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include <stdio.h>
#include "../cgame/cg_local.h"
#include "../cgame/tce_reload_event.h"
static int calls,args[4];
static void require(int ok) { if(!ok) { fprintf(stderr,"reload event comparison failed\n"); exit(1); } }
void trap_S_StartSoundVControl(vec3_t origin,int entity,int channel,sfxHandle_t sound,int volume) {
    require(origin==NULL); ++calls;args[0]=entity;args[1]=channel;args[2]=sound;args[3]=volume;
}
static int lookup(void *a,void *b) { return 0; }
static void hook(unsigned char *p,void *fn) {
    DWORD old,tmp; require(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old));
    p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;
    FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);
}
int main(int argc,char **argv) {
    HMODULE dll;unsigned char *base,cent[0x1000],expected[0x1000];
    tce_fragmentSoundContext_t c;int sounds[4],e,w,i,j,n=0,pending,expectedCalls,expectedArgs[4];
    void (__cdecl *original)(void *,void *);
    require(argc==2);dll=LoadLibraryA(argv[1]);require(dll!=NULL);base=(unsigned char *)dll;
    original=(void *)(base+0x37d40);
    hook(base+0x534e0,lookup);hook(base+0x19120,lookup);
    hook(base+0x64fe0,trap_S_StartSoundVControl);
    *(int *)(base+0x40128cc)=0;
    /* Leave original SoundVolume and all of its math intact. */
    for(e=130;e<=133;++e)for(w=0;w<64;++w)for(i=0;i<256;++i) {
        memset(cent,0,sizeof(cent));memset(&c,0,sizeof(c));
        *(int *)cent=i%64;*(int *)(cent+0xac)=i%67-1;
        *(int *)(cent+0xb8)=e|((i%4)<<8);*(int *)(cent+0xe8)=w;
        pending=i%3-1;*(int *)(cent+0xa7c)=pending;
        c.listener[0]=17;c.listener[1]=-32;c.listener[2]=50;
        ((float *)(cent+0x18))[0]=(float)(i%32)*80;
        c.distanceVariant=(i>>2)&1;c.attenuation=(float)((i>>3)%5)*.3f;c.disabled=(i>>1)&1;
        memcpy(base+0x407f01c,c.listener,12);*(int *)(base+0x3fdf380)=c.distanceVariant;
        *(float *)(base+0x40a49cc)=c.attenuation;*(int *)(base+0x40a4a1c)=c.disabled;
        for(j=0;j<4;++j) sounds[j]=i&1?100+j:0;
        memcpy(base+0x2588ab4,sounds,sizeof(sounds));
        *(int *)(base+0x4c1df20+w*0x1cc+0x170)=(i>>4)&1;
        *(int *)(base+0x4c1df20+w*0x1cc+0x16c)=(i>>5)&1;
        memcpy(expected,cent,sizeof(cent));calls=0;memset(args,0,sizeof(args));
        original(cent,NULL);expectedCalls=calls;memcpy(expectedArgs,args,sizeof(args));
        calls=0;memset(args,0,sizeof(args));
        TCE_CG_ReloadEvent(e,*(int *)cent,(float *)(cent+0x18),(i>>4)&1,(i>>5)&1,sounds,&c,&pending);
        require(calls==expectedCalls&&!memcmp(args,expectedArgs,sizeof(args)));
        require(pending==*(int *)(cent+0xa7c));*(int *)(expected+0xa7c)=pending;
        require(!memcmp(expected,cent,sizeof(cent)));++n;
    }
    FreeLibrary(dll);printf("%d original reload phase event comparisons passed.\n",n);return 0;
}
