#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
static byte raw[4][0x57c],rawClient[0x2000];static gentity_t entities[4];static gclient_t client;static int mode,bad,count,events[32],present,otherPresent,targetPhase,removeAt,findIndex;
static char targetName[]="test_target";
static void*entity(int i){return mode?(void*)raw[i]:(void*)&entities[i];}
static int hook(void*p,void*f){DWORD old,tmp;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);return 1;}
static void used(gentity_t*e,gentity_t*o,gentity_t*a){int i;for(i=0;i<4&&e!=entity(i);++i){}events[count++]=10+i;if(i==4||o!=(targetPhase?entity(0):otherPresent?entity(2):NULL)||a!=(present?entity(3):NULL))bad=1;if(targetPhase&&i==removeAt){if(mode)*(int*)(raw[0]+0x194)=0;else entities[0].inuse=0;}}
static void door(gentity_t*e,gentity_t*o,gentity_t*a){int before=count;used(e,o,a);events[before]+=10;}
static gentity_t*find(gentity_t*from,const char*name,int hash){events[count++]=30+findIndex;if(from!=(findIndex?entity(findIndex-1):NULL)||name!=targetName||hash!=BG_StringHashValue(targetName))bad=1;return findIndex<3?entity(findIndex++):NULL;}
static void print(const char*fmt,...){if(!strcmp(fmt,"WARNING: Entity used itself.\n"))events[count++]=40;else if(!strcmp(fmt,"entity was removed while using targets\n"))events[count++]=41;else bad=1;}
int main(int argc,char**argv){HMODULE dll;byte*b;int(__cdecl*allow)(void*,void*);void(__cdecl*use)(void*,void*,void*);void(__cdecl*targets)(void*,void*);int mask,team,disguise,expected,k,n=0,expectedEvents[32],expectedCount;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;allow=(void*)(b+0x94af0);use=(void*)(b+0x94b60);targets=(void*)(b+0x94b90);
 for(mask=0;mask<32;++mask)for(team=-1;team<6;++team)for(disguise=-1;disguise<2;++disguise)for(present=0;present<3;++present)for(otherPresent=0;otherPresent<2;++otherPresent){entities[0].allowteams=*(int*)(raw[0]+0x4ac)=mask;entities[3].client=present==2?&client:NULL;*(void**)(raw[3]+0x190)=present==2?rawClient:NULL;client.sess.sessionTeam=*(int*)(rawClient+0xb30)=team;client.ps.powerups[PW_OPS_DISGUISED]=*(int*)(rawClient+0x170)=disguise;entities[0].use=used;*(void**)(raw[0]+0x2b4)=used;
  mode=1;expected=allow(entity(0),present?entity(3):NULL);mode=0;if(G_AllowTeamsAllowed(&entities[0],present?&entities[3]:NULL)!=expected){fprintf(stderr,"allow case%d mismatch\n",n);return 1;}
  mode=1;bad=count=0;use(entity(0),otherPresent?entity(2):NULL,present?entity(3):NULL);expectedCount=count;memcpy(expectedEvents,events,sizeof(events));if(bad)return 3;mode=0;bad=count=0;G_UseEntity(entity(0),otherPresent?entity(2):NULL,present?entity(3):NULL);if(bad||count!=expectedCount||memcmp(events,expectedEvents,count*sizeof(int))){fprintf(stderr,"use case%d mismatch\n",n);return 1;}++n;}
 printf("%d whole G_AllowTeamsAllowed and %d whole G_UseEntity comparisons passed\n",n,n);
 if(!hook(b+0x949f0,find)||!hook(G_FindByTargetnameFast,find)||!hook(b+0x70d20,door)||!hook(G_TryDoor,door)||!hook(b+0x5ed80,print)||!hook(G_Printf,print))return 2;
 targetPhase=1;
 for(k=0;k<4096;++k){int i;static const char*names[]={"normal","func_door","FUNC_DOOR_ROTATING","func_door_extra"};memset(raw,0,sizeof(raw));memset(entities,0,sizeof(entities));present=k&1;removeAt=(k/2)%4-1;entities[0].inuse=*(int*)(raw[0]+0x194)=1;entities[0].target=*(char**)(raw[0]+0x254)=(k/1024)%2?NULL:targetName;entities[0].flags=*(int*)(raw[0]+0x1b0)=((k/8)%2?FL_KICKACTIVATE:0)|((k/16)%2?FL_SOFTACTIVATE:0);
  for(i=1;i<3;++i){entities[i].classname=*(char**)(raw[i]+0x1a4)=(char*)names[(k/(i==1?32:128))%4];entities[i].use=(k/(i==1?512:256))%2?NULL:used;*(void**)(raw[i]+0x2b4)=entities[i].use;entities[i].flags=*(int*)(raw[i]+0x1b0)=0x200;}
  mode=1;bad=count=findIndex=0;targets(k>=2048?NULL:entity(0),present?entity(3):NULL);expectedCount=count;memcpy(expectedEvents,events,sizeof(events));if(bad){fprintf(stderr,"original targets callback case%d\n",k);return 3;}mode=0;bad=count=findIndex=0;G_UseTargets(k>=2048?NULL:entity(0),present?entity(3):NULL);
  if(bad||count!=expectedCount||memcmp(events,expectedEvents,count*sizeof(int))||entities[0].inuse!=*(int*)(raw[0]+0x194)||entities[1].flags!=*(int*)(raw[1]+0x1b0)||entities[2].flags!=*(int*)(raw[2]+0x1b0)){fprintf(stderr,"targets case%d mismatch count%d/%d bad%d\n",k,count,expectedCount,bad);return 1;}}
 puts("4096 whole G_UseTargets comparisons passed");return 0;
}
