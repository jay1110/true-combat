#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include "../game/tce_bg.h"
#include <windows.h>
#include <stdio.h>
static byte rawEnt[0x57c],rawClient[0x2000],rawItem[0x57c],rawPopup[0x57c],rawManager[0x57c];static gentity_t ent,item,popup,manager;static gclient_t client;static int mode,bad,count,events[8],mutate;
extern int Pickup_Ammo(gentity_t*,gentity_t*);
static int hook(void*p,void*f){DWORD old,tmp;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);return 1;}
static gentity_t*message(popupMessageType_t t){events[count++]=1;if(t!=4)bad=1;return mode?(void*)rawPopup:&popup;}
static void script(gentity_t*e,char*ev,char*param){events[count++]=2;if(e!=(mode?(void*)rawManager:&manager)||strcmp(ev,"trigger")||strcmp(param,"bomb_secured"))bad=1;}
static void spawn(gclient_t*c){events[count++]=3;if(c!=(mode?(void*)rawClient:&client))bad=1;if(mutate){if(mode){*(int*)(rawClient+0xa0)=57;*(int*)(rawClient+0x11c)=2;}else{client.ps.clientNum=57;client.ps.persistant[PERS_TEAM]=2;}}}
int main(int argc,char**argv){HMODULE dll;byte*b;int(__cdecl*magic)(void*,int);int(__cdecl*pickup)(void*,void*);int k,j,n=0;const int clipCounts[]={-2,-1,0,1,2,7};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;magic=(void*)(b+0x5c900);pickup=(void*)(b+0x5c770);ent.client=&client;*(void**)(rawEnt+0x190)=rawClient;
 for(k=0;k<64000;++k){int x,y,clips=clipCounts[(k/64)%6];memset(&client,0,sizeof(client));memset(rawClient,0,sizeof(rawClient));client.sess.sessionTeam=*(int*)(rawClient+0xb30)=(k/384)%4;client.ps.stats[STAT_PLAYER_CLASS]=*(int*)(rawClient+0xe4)=(k/1536)%5;
  for(j=0;j<SK_NUM_SKILLS;++j)client.sess.skill[j]=(k/31+j)%6-1;memcpy(rawClient+0xba4,client.sess.skill,sizeof(client.sess.skill));
  COM_BitSet(client.ps.weapons,k%64);if(k%3==0)COM_BitSet(client.ps.weapons,11);memcpy(rawClient+0x3d4,client.ps.weapons,8);
  for(j=0;j<64;++j){client.ps.ammo[j]=(k+j)%151-3;client.ps.ammoclip[j]=(k*3+j)%41-3;weaponDef[j].maxclip=(k+j)%35;weaponDef[j].maxammo=(k+j)%151;}memcpy(b+0x24435c0,weaponDef,sizeof(weaponDef));memcpy(rawClient+0x190,client.ps.ammo,256);memcpy(rawClient+0x290,client.ps.ammoclip,256);
  x=magic(rawEnt,clips);y=AddMagicAmmo(&ent,clips);if(x!=y||memcmp(rawClient+0x190,client.ps.ammo,256)||memcmp(rawClient+0x290,client.ps.ammoclip,256)||memcmp(rawClient+0x3d4,client.ps.weapons,8)){fprintf(stderr,"magic case%d slot%d clips%d mismatch %d/%d\n",k,k%64,clips,x,y);return 1;}++n;
 }
 printf("%d AddMagicAmmo whole-wrapper comparisons with real64-slot dependency passed\n",n);
 if(!hook(b+0x95050,message)||!hook(G_PopupMessage,message)||!hook(b+0x7d4b0,script)||!hook(G_Script_ScriptEvent,script)||!hook(b+0x4b9c0,spawn)||!hook(SetWolfSpawnWeapons,spawn))return 2;
 for(k=0;k<4096;++k){int x,y,expected[8],expectedCount;static const char*names[]={"ammo_dynamite","AMMO_DYNAMITE","ammo_VIP","AMMO_vip","ammo_9mm",""};memset(rawPopup,0,sizeof(rawPopup));memset(&popup,0,sizeof(popup));memset(&client,0,sizeof(client));memset(rawClient,0,sizeof(rawClient));item.classname=*(char**)(rawItem+0x1a4)=(char*)names[k%6];client.ps.clientNum=*(int*)(rawClient+0xa0)=k%64;client.ps.persistant[PERS_TEAM]=*(int*)(rawClient+0x11c)=(k/6)%4;client.ps.stats[STAT_TCE_WEAPON_FLAGS]=*(int*)(rawClient+0xf0)=k&0x1ff;client.ps.stats[STAT_TCE_FLAGS]=*(int*)(rawClient+0xf4)=k&0x3ff;
  level.tceBombCarrier=*(int*)(b+0x222391c)=(k/24)%3-1;level.tceVipCarrier=*(int*)(b+0x2223948)=(k/72)%3-1;level.tceBombCarrierCount=*(int*)(b+0x2223938)=k%17;level.tceDemolitionTeam=*(int*)(b+0x222393c)=(k/216)%4;level.tceExitRulesNotBefore=*(int*)(b+0x2223928)=-37;client.tceBombPossessionOrder=*(int*)(rawClient+0x1328)=-9;level.gameManager=&manager;*(void**)(b+0x221b71c)=rawManager;level.time=*(int*)(b+0x7febe0)=100000+k;mutate=k%2;
  mode=1;bad=count=0;x=pickup(rawItem,rawEnt);if(bad)return 3;expectedCount=count;memcpy(expected,events,sizeof(events));mode=0;bad=count=0;y=Pickup_Ammo(&item,&ent);
  if(bad||x!=y||count!=expectedCount||memcmp(events,expected,count*sizeof(int))||client.ps.stats[STAT_TCE_WEAPON_FLAGS]!=*(int*)(rawClient+0xf0)||client.ps.stats[STAT_TCE_FLAGS]!=*(int*)(rawClient+0xf4)||client.tceBombPossessionOrder!=*(int*)(rawClient+0x1328)||level.tceBombCarrier!=*(int*)(b+0x222391c)||level.tceVipCarrier!=*(int*)(b+0x2223948)||level.tceBombCarrierCount!=*(int*)(b+0x2223938)||level.tceExitRulesNotBefore!=*(int*)(b+0x2223928)||popup.s.effect2Time!=*(int*)(rawPopup+0x110)||popup.s.effect3Time!=*(int*)(rawPopup+0x114)||popup.s.density!=*(int*)(rawPopup+0xf4)){fprintf(stderr,"objective pickup case%d mismatch\n",k);return 1;}
 }
 puts("4096 whole Pickup_Ammo valid-manager comparisons passed");return 0;
}
