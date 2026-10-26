#include "../game/g_local.h"
#include "../game/tce_bg.h"
#include <windows.h>
#include <stdio.h>
int main(int argc,char **argv) {
 HMODULE dll;unsigned char *base;int (__cdecl *original)(unsigned);
 unsigned high,low,flags,seed=173,n=0;
 if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;base=(unsigned char *)dll;
 original=(void *)(base+0x2b330);
 for(high=0;high<256;++high)for(low=0;low<256;++low) {
  flags=(high<<24)|low;
  if(original(flags)!=BG_SurfaceFlag2Type(flags))return 1;
  ++n;
 }
 for(low=0;low<1000000;++low) {
  seed=seed*1664525u+1013904223u;
  if(original(seed)!=BG_SurfaceFlag2Type(seed))return 1;
  ++n;
 }
 if(memcmp(tcePierceTable,base+0xb66b0,sizeof(tcePierceTable)))return 1;
 printf("%u original material conversions and all37 pierceTable entries matched\n",n);
 return 0;
}
