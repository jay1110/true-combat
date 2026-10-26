#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
int main(int argc,char **argv) {
 HMODULE dll; byte *b; void (__cdecl *orig[2])(void*,float*);
 static byte raw[0x57c],client[4096];static gentity_t ent;static gclient_t cl;
 int k,op,has,alias,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;
 orig[0]=(void*)(b+0x951d0);orig[1]=(void*)(b+0x95250);
 for(k=0;k<1000;++k)for(op=0;op<2;++op)for(has=0;has<2;++has)for(alias=0;alias<4;++alias) {
  vec3_t a,v;float *inRaw,*inSource;
  memset(raw,0xa5,sizeof(raw));memset(client,0xa5,sizeof(client));memset(&ent,0xa5,sizeof(ent));memset(&cl,0xa5,sizeof(cl));
  ent.client=has?&cl:NULL;*(void**)(raw+400)=has?client:NULL;
  VectorSet(v,(k-500)*.125f,(k%73-36)*1.25f,(k%17-8)*.0625f);VectorCopy(v,a);
  inSource=alias==0?v:alias==1?(op?ent.s.apos.trBase:ent.s.pos.trBase):alias==2?(op?ent.r.currentAngles:ent.r.currentOrigin):(op?ent.s.apos.trDelta:ent.s.pos.trDelta);
  inRaw=alias==0?a:(float*)(raw+(alias==1?(op?0x3c:0x18):alias==2?(op?0x174:0x168):(op?0x48:0x24)));
  VectorCopy(v,inSource);VectorCopy(a,inRaw);
  orig[op](raw,inRaw);if(op)G_SetAngle(&ent,inSource);else G_SetOrigin(&ent,inSource);
  if(memcmp(&ent.s.pos,raw+12,36)||memcmp(&ent.s.apos,raw+48,36)||memcmp(ent.r.currentOrigin,raw+0x168,12)||memcmp(ent.r.currentAngles,raw+0x174,12)||memcmp(cl.ps.origin,client+20,12)) {
   printf("transform mismatch case%d op%d client%d alias%d\n",k,op,has,alias);return 1;
  }++n;
 }
 printf("%d whole entity transform comparisons passed\n",n);FreeLibrary(dll);return 0;
}
