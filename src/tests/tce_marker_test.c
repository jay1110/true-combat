#define WIN32_LEAN_AND_MEAN
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
/* Original DLL rows have an 80-byte prefix; the mod appends lean/weapon. */
#define LEGACY_MARKER_BYTES 80
static unsigned char rawClient[0x16b0],rawEnt[0x57c];
static gclient_t client;
static gentity_t ent;
static int milliseconds,calls;
static void patch(void *at,void *fn){DWORD o,t;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static int millis(void){++calls;return milliseconds;}
static void pose(unsigned char *b,int v){int j;
 ent.client=&client;*(void **)(rawEnt+0x190)=rawClient;
 for(j=0;j<3;++j){
  ent.r.mins[j]=*(float *)(rawEnt+0x134+j*4)=-v-j*.5f;
  ent.r.maxs[j]=*(float *)(rawEnt+0x140+j*4)=v*3+j*.25f;
  ent.r.currentOrigin[j]=*(float *)(rawEnt+0x168+j*4)=v*41+j*.75f;
  ent.s.pos.trBase[j]=9999; /* Must use currentOrigin, not the trajectory. */
  ent.r.currentAngles[j]=*(float *)(rawEnt+0x174+j*4)=v*13+j*2.5f;
  client.ps.viewangles[j]=*(float *)(rawClient+0xb0+j*4)=v*7+j*3.25f;
 }
 client.ps.pm_flags=*(int *)(rawClient+0xc)=v*71;
 client.ps.stats[STAT_TCE_WEAPON_FLAGS]=*(int *)(rawClient+0xf0)=v*31;
 client.ps.eFlags=*(int *)(rawClient+0x68)=v*17;
 client.ps.leanf=*(float *)(rawClient+0x3c)=v*1.25f;
 client.ps.weapon=*(int *)(rawClient+0xa4)=v%64;
 ent.r.svFlags=*(int *)(rawEnt+0x128)=v&1?SVF_BOT:0;
 ent.inuse=v&1;ent.r.linked=v&2;ent.health=v&4?0:100;
 client.ps.pm_type=v%5;client.sess.sessionTeam=v%4;
}
static int same(void){int row;if(client.topMarker!=*(int *)(rawClient+0x133c))return 0;
 for(row=0;row<MAX_CLIENT_MARKERS;++row)if(memcmp(&client.clientMarkers[row],rawClient+0x1340+row*LEGACY_MARKER_BYTES,LEGACY_MARKER_BYTES))return 0;
 return 1;}
static int captured(int row){return client.clientMarkers[row].lean==client.ps.leanf&&client.clientMarkers[row].weapon==client.ps.weapon;}
int main(int argc,char **argv){HMODULE h;unsigned char *b;int top,v,t,m,f,j,a,c,cases=0;const int ages[]={-100,0,1,50,500};const int ms[]={-100,0,1,49,50,51,1000};
 if(offsetof(clientMarker_t,lean)!=LEGACY_MARKER_BYTES)return 2;if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
 patch(b+0x8ab50,millis);patch(trap_Milliseconds,millis);
 for(v=0;v<32;++v){pose(b,v);level.time=*(int *)(b+0x7febe0)=v*47-20;memset(client.clientMarkers,0x35,sizeof(client.clientMarkers));memset(rawClient+0x1340,0x35,800);calls=0;
  ((void(__cdecl *)(void *))(b+0x4e3a0))(rawEnt);G_ResetMarkers(&ent);if(!same()||calls){printf("reset mismatch %d\n",v);return 1;}
  for(a=0;a<MAX_CLIENT_MARKERS;++a)if(!captured(a)){printf("extended reset mismatch %d row%d\n",v,a);return 1;}++cases;
 }
 for(top=0;top<10;++top)for(v=0;v<8;++v)for(t=0;t<5;++t)for(m=0;m<7;++m)for(f=0;f<3;++f){
  pose(b,v);level.time=*(int *)(b+0x7febe0)=10000;level.previousTime=*(int *)(b+0x7febe4)=f==0?9950:f==1?10000:10001;level.frameTime=*(int *)(b+0x2223968)=20000;milliseconds=20000+ms[m];
  memset(client.clientMarkers,0x35,sizeof(client.clientMarkers));memset(rawClient+0x1340,0x35,800);client.topMarker=*(int *)(rawClient+0x133c)=top;client.clientMarkers[top].serverTime=*(int *)(rawClient+0x1380+top*80)=10000-ages[t];
  calls=0;((void(__cdecl *)(void *))(b+0x4e560))(rawEnt);a=calls;calls=0;G_StoreClientPosition(&ent);c=calls;
  if(!same()||a!=c||!captured(client.topMarker)){printf("store mismatch top%d v%d age%d ms%d frame%d\n",top,v,ages[t],ms[m],f);return 1;}++cases;
 }
 /* Ring wrap and repeated updates across a continuous sequence of frames. */
 pose(b,2);level.time=*(int *)(b+0x7febe0)=1000;((void(__cdecl *)(void *))(b+0x4e3a0))(rawEnt);G_ResetMarkers(&ent);
 for(j=0;j<240;++j){pose(b,j%16);level.previousTime=*(int *)(b+0x7febe4)=1000+(j/3)*50;level.time=*(int *)(b+0x7febe0)=level.previousTime+50;level.frameTime=*(int *)(b+0x2223968)=20000+j;milliseconds=level.frameTime+(j%3)*17;
  ((void(__cdecl *)(void *))(b+0x4e560))(rawEnt);G_StoreClientPosition(&ent);if(!same()||!captured(client.topMarker)){printf("sequence mismatch %d\n",j);return 1;}++cases;
 }
 printf("%d original marker-prefix reset/capture comparisons and extended lean/weapon contracts passed\n",cases);return 0;
}
