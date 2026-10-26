#include "../game/tce_bg.h"
#include <windows.h>
#include <stdio.h>
/* Exercise the real renderer's eight-argument ABI, stopping at its sky guard.
 * Only sound and material decoding are intercepted; no rendering is claimed. */
static int soundCalls,decodeCalls;
static unsigned decodedMaterial;
static float *expectedOrigin;
static int soundVolume(float *origin,float volume,float range,int linear) {
 if(origin!=expectedOrigin || volume!=127 || range!=1800 || linear)abort();
 ++soundCalls;return 127;
}
static unsigned decodeMaterial(unsigned material) {
 ++decodeCalls;decodedMaterial=material;return 4;
}
static void hook(unsigned char *p,void *fn) {
 DWORD old,tmp;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);
 p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;
 FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);
}
int main(int argc,char **argv) {
 HMODULE dll;unsigned (__cdecl *original)(unsigned);
 unsigned i,seed=19,n=0;int weapon,encoded,abiCases=0;
 unsigned char *base;
 void (__cdecl *renderer)(int,int,float *,float *,float *,unsigned,int,int);
 float origin[3]={10,20,30},normal[3]={0,0,1},direction[3]={1,0,0};
 if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;
 original=(void *)((unsigned char *)dll+0x3b50);
 for(i=0;i<65536;++i) {
  if(original(i)!=BG_SurfaceType2Flag(i))return 1;
  ++n;
 }
 for(i=0;i<1000000;++i) {
  seed=seed*1664525u+1013904223u;
  if(original(seed)!=BG_SurfaceType2Flag(seed))return 1;
  ++n;
 }
 printf("%u original material-to-flag comparisons matched\n",n);
 base=(unsigned char *)dll;renderer=(void *)(base+0x78260);
 hook(base+0x44b50,soundVolume);hook(base+0x3b50,decodeMaterial);
 *(int *)(base+0x40a4a1c)=0;expectedOrigin=origin;
 for(weapon=0;weapon<67;++weapon)for(encoded=-1;encoded<=1;++encoded)
 for(i=0;i<37;++i) {
  unsigned material=(weapon==1 || encoded)?i:4;
  soundCalls=decodeCalls=0;
  renderer(weapon,1,origin,normal,direction,material,1,encoded);
  if(soundCalls!=1 || decodeCalls!=(weapon==1 || encoded!=0) ||
     (decodeCalls && decodedMaterial!=material))return 1;
  ++abiCases;
 }
 printf("%d original eight-argument renderer prelude calls matched\n",abiCases);
 return 0;
}
