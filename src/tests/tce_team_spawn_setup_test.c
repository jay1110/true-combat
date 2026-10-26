#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern void SP_team_CTF_redspawn(gentity_t*),SP_team_CTF_bluespawn(gentity_t*),SP_team_hostage_spawn(gentity_t*);
extern void Use_Team_Spawnpoint(gentity_t*,gentity_t*,gentity_t*),DropToFloor(gentity_t*);
static byte *base,raw[0x57c],targetRaw[0x57c];static gentity_t ent,target;
static int side,variant,bad,count;static int events[16];
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static void note(int n){if(count>=16)exit(3);events[count++]=n;}
static void*pick(char*s){note(1);if(strcmp(s,"target"))bad=1;return variant&1?(side?(void*)&target:(void*)targetRaw):NULL;}
static void angles(const float*d,float*out){int k;note(2);for(k=0;k<3;k++){float expected=(float)(k*10+variant)-(float)(k*2-variant);if(d[k]!=expected)bad=1;out[k]=d[k]*.5f+3;}}
static int spawn(char*k,char*d,int*out,char*f,int line){note(3);if(strcmp(k,"spawngroup")||strcmp(d,"0")||out!=(side?&ent.tceObjectiveScore:(int*)(raw+0x2e4)))bad=1;*out=variant-300;return variant&2;}
static void freeEntity(void*p){note(4);if(p!=(side?(void*)&ent:(void*)raw))bad=1;if(side)ent.inuse=qfalse;else*(int*)(raw+0x194)=0;}
typedef struct{int n,log[16],score,inuse,enemy,use,think;float mins[3],maxs[3],angles[3];} result;
int main(int argc,char**argv){HMODULE dll;int mode,v,s,k,n=0;void(__cdecl*original[3])(void*);void(*source[3])(gentity_t*)={SP_team_CTF_redspawn,SP_team_CTF_bluespawn,SP_team_hostage_spawn};result a,b;void(*hostageUse)(gentity_t*,gentity_t*,gentity_t*)=NULL;const char*maps[]={"obj_snow","OBJ_SNOW","obj_snow2","obj_northport"};float coords[][3]={{2334,1002,-104},{2334,1266,-104},{2335,1002,-104},{2334,1003,-104},{2334,1266,-103}};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;original[0]=(void*)(base+0x8e6a0);original[1]=(void*)(base+0x8e770);original[2]=(void*)(base+0x8e8a0);
 if(!hook(base+0x94a70,pick)||!hook(G_PickTarget,pick)||!hook(base+0xa05c0,angles)||!hook(vectoangles,angles)||!hook(base+0x85a30,spawn)||!hook(G_SpawnIntExt,spawn)||!hook(base+0x94f30,freeEntity)||!hook(G_FreeEntity,freeEntity))return 2;
 for(mode=0;mode<3;mode++)for(v=0;v<1280;v++)for(s=0;s<2;s++){
 side=s;variant=v;bad=count=0;memset(events,0,sizeof(events));memset(raw,0,sizeof(raw));memset(targetRaw,0,sizeof(targetRaw));memset(&ent,0,sizeof(ent));memset(&target,0,sizeof(target));memset(&b,0,sizeof(b));
 ent.target="target";*(char**)(raw+0x254)="target";ent.inuse=*(int*)(raw+0x194)=1;
 for(k=0;k<3;k++){ent.s.origin[k]=*(float*)(raw+0x5c+k*4)=(float)(k*2-v);target.s.origin[k]=*(float*)(targetRaw+0x5c+k*4)=(float)(k*10+v);ent.s.angles[k]=*(float*)(raw+0x74+k*4)=99+k;ent.r.currentOrigin[k]=*(float*)(raw+0x168+k*4)=coords[(v/8)%5][k];}
 strcpy(level.rawmapname,maps[(v/2)%4]);strcpy((char*)base+0x7feb98,maps[(v/2)%4]);if(s)source[mode](&ent);else original[mode](raw);
 b.n=count;memcpy(b.log,events,sizeof(events));b.score=s?ent.tceObjectiveScore:*(int*)(raw+0x2e4);b.inuse=s?ent.inuse:*(int*)(raw+0x194);b.enemy=s?(ent.enemy==&target):(*(void**)(raw+0x2ec)==targetRaw);
 b.think=s?(ent.think==DropToFloor):(*(void**)(raw+0x2a4)==base+0x76340);b.use=s?(mode==2?ent.use!=NULL:ent.use==Use_Team_Spawnpoint):(*(void**)(raw+0x2b4)==base+(mode==2?0x8e660:0x8e5f0));
 memcpy(b.mins,s?ent.r.mins:(float*)(raw+0x134),12);memcpy(b.maxs,s?ent.r.maxs:(float*)(raw+0x140),12);memcpy(b.angles,s?ent.s.angles:(float*)(raw+0x74),12);
 if(!s)a=b;else{if(bad||memcmp(&a,&b,sizeof(a))){printf("spawn mismatch mode%d case%d events%d/%d\n",mode,v,a.n,b.n);return 1;}if(mode==2)hostageUse=ent.use;n++;}}
 for(v=0;v<25;v++){int active=v%5-2,secured=v/5-2;level.tceHostageActive=*(int*)(base+0x2223950)=active;level.tceHostageSecured=*(int*)(base+0x2223954)=secured;((void(__cdecl*)(void*,void*,void*))(base+0x8e660))(NULL,NULL,NULL);hostageUse(NULL,NULL,NULL);if(level.tceHostageActive!=*(int*)(base+0x2223950)||level.tceHostageSecured!=*(int*)(base+0x2223954)){printf("hostage use mismatch %d\n",v);return 1;}}
 printf("PASS %d spawn and 25 hostage activation cases\n",n);return 0;
}
