#define WIN32_LEAN_AND_MEAN
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <limits.h>
extern qboolean IsHeadShotWeapon(int);
static unsigned char rawEnt[0x57c],rawClient[0x16b0],rawPart[0x57c];
static gentity_t actor,part;static gclient_t client;
static int tagResult,spawnCount,tagCount,linkCount;static orientation_t tag;
static void patch(void *at,void *fn){DWORD o,t;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void *spawnOriginal(void){++spawnCount;memset(rawPart,0,sizeof(rawPart));return rawPart;}
static gentity_t *spawnSource(void){++spawnCount;memset(&part,0,sizeof(part));return &part;}
static int getTag(int clientnum,int file,const char *name,orientation_t *out){++tagCount;if(clientnum!=17||file||strcmp(name,"tag_head"))abort();*out=tag;return tagResult;}
static void link(void *e){++linkCount;}
static int sameFloat(float a,float b){return fabs(a-b)<.00015f;}
static int compare(int kind) {
 int j;
 for(j=0;j<3;++j) {
  if(!sameFloat(*(float *)(rawPart+0x168+4*j),part.r.currentOrigin[j])||!sameFloat(*(float *)(rawPart+0x5c+4*j),part.s.origin[j])||
     !sameFloat(*(float *)(rawPart+0x134+4*j),part.r.mins[j])||!sameFloat(*(float *)(rawPart+0x140+4*j),part.r.maxs[j])||
     !sameFloat(*(float *)(rawPart+0x74+4*j),part.s.angles[j])||!sameFloat(*(float *)(rawPart+0x3c+4*j),part.s.apos.trBase[j])||
     !sameFloat(*(float *)(rawPart+0x48+4*j),part.s.apos.trDelta[j])||!sameFloat(*(float *)(rawPart+0x18+4*j),part.s.pos.trBase[j]))return 0;
 }
 return *(int *)(rawPart+0x1d4)==part.clipmask && *(int *)(rawPart+0x14c)==part.r.contents &&
        *(void **)(rawPart+0x218)==rawEnt && part.parent==&actor &&
        *(int *)(rawPart+4)==(kind?45:43) && part.s.eType==(kind?ET_TEMPLEGS:ET_TEMPHEAD);
}
int main(int argc,char **argv) {
 HMODULE h;unsigned char *base;int n,v,j,kind,prone,duck,tagged,height,expectedSpawn,expectedTag,expectedLink,count=0,mod;
 float pitches[]={-90,-1,0,45,90,179,180,181,270,359,360};void *ret;gentity_t *result;
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;base=(unsigned char *)h;
 patch(base+0x94e30,spawnOriginal);patch(G_Spawn,spawnSource);patch(base+0x8b0c0,getTag);patch(trap_GetTag,getTag);patch(base+0x8afa0,link);patch(trap_LinkEntity,link);
 for(n=0;n<771;++n){mod=n<769?n-256:n==769?INT_MIN:INT_MAX;if(((int(__cdecl *)(int))(base+0x58590))(mod)!=IsHeadShotWeapon(mod)){printf("MOD mismatch%d\n",mod);return 1;}++count;}
 for(kind=0;kind<2;++kind)for(prone=0;prone<2;++prone)for(duck=0;duck<2;++duck)for(tagged=0;tagged<2;++tagged)for(height=0;height<4;++height)for(v=0;v<11;++v) {
 memset(rawEnt,0,sizeof(rawEnt));memset(rawClient,0,sizeof(rawClient));memset(&actor,0,sizeof(actor));memset(&client,0,sizeof(client));
 actor.client=&client;*(void **)(rawEnt+0x190)=rawClient;*(int *)rawEnt=actor.s.number=17;
 *(int *)(rawClient+0x68)=client.ps.eFlags=prone?EF_PRONE:0;*(int *)(rawClient+0xc)=client.ps.pm_flags=duck?PMF_DUCKED:0;
 *(int *)(rawClient+0xbc)=client.ps.viewheight=height*15+10;*(float *)(rawClient+0x3f8)=client.ps.crouchViewHeight=height*8.25f+14;
 *(float *)(rawClient+0x1274)=client.pmext.proneLegsOffset=height*5.75f-7;
 for(j=0;j<3;++j){*(float *)(rawEnt+0x168+4*j)=actor.r.currentOrigin[j]=-34.25f+j*79;
 *(float *)(rawEnt+0x174+4*j)=actor.r.currentAngles[j]=j*54.75f-140;
 *(float *)(rawClient+0xb0+4*j)=client.ps.viewangles[j]=j==0?pitches[v]:j==1?v*31.25f-180:height*15.5f;
 tag.origin[j]=j*34.75f-91;}
 tagResult=tagged;spawnCount=tagCount=linkCount=0;
 ret=((void *(__cdecl *)(void *))(base+(kind?0x58860:0x58600)))(rawEnt);expectedSpawn=spawnCount;expectedTag=tagCount;expectedLink=linkCount;
 spawnCount=tagCount=linkCount=0;result=kind?G_BuildLeg(&actor):G_BuildHead(&actor);
 if(!!ret!=!!result||expectedSpawn!=spawnCount||expectedTag!=tagCount||expectedLink!=linkCount||(ret&&!compare(kind))){printf("Geometry mismatch kind%d prone%d duck%d tag%d height%d pitch%g calls%d,%d,%d/%d,%d,%d origin%g,%g,%g/%g,%g,%g\n",kind,prone,duck,tagged,height,pitches[v],expectedSpawn,expectedTag,expectedLink,spawnCount,tagCount,linkCount,*(float *)(rawPart+0x168),*(float *)(rawPart+0x16c),*(float *)(rawPart+0x170),part.r.currentOrigin[0],part.r.currentOrigin[1],part.r.currentOrigin[2]);return 1;}
 ++count;
 }
 printf("PASS %d complete Windows MOD/head/prone-leg production comparisons\n",count);return 0;
}

