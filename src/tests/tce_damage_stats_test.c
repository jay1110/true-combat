#define WIN32_LEAN_AND_MEAN
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <limits.h>
static unsigned char entity[2][0x57c], clients[2][0x1600];
static gentity_t actors[2];static gclient_t sourceClients[2];
static int match(int i) {
 return !memcmp(clients[i]+0xbe4,&sourceClients[i].sess.damage_given,12*sizeof(int)) &&
 !memcmp(clients[i]+0xc14,sourceClients[i].sess.aWeaponStats,sizeof(sourceClients[i].sess.aWeaponStats)) &&
 *(int *)(clients[i]+0xdf8)==sourceClients[i].sess.tceSessionValues[3];
}
int main(int argc,char **argv) {
 HMODULE h;unsigned char *base;int mod,i,j,state,gt,kind,attack,flags,dmg,n=0;
 int damage[]={-3,0,17};unsigned int index;
 unsigned int (__cdecl *map)(unsigned int);void (__cdecl *stats)(void *,void *,int,int);
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;base=(unsigned char *)h;
 map=(void *)(base+0x656c0);stats=(void *)(base+0x65470);
 for(i=0;i<1027;++i){index=i<1025?i-512:i==1025?INT_MIN:INT_MAX;
  if(map(index)!=G_weapStatIndex_MOD(index)){printf("MOD stat index mismatch %u\n",index);return 1;}++n;}
 for(state=0;state<2;++state)for(gt=0;gt<3;++gt)for(kind=0;kind<5;++kind)
 for(attack=0;attack<4;++attack)for(flags=0;flags<4;++flags)for(mod=-1;mod<80;++mod)for(dmg=0;dmg<3;++dmg){
  memset(entity,0,sizeof(entity));memset(clients,0,sizeof(clients));memset(actors,0,sizeof(actors));memset(sourceClients,0,sizeof(sourceClients));
  *(int *)(base+0x784f6c)=g_gamestate.integer=state;*(int *)(base+0x6d8d4c)=g_gametype.integer=gt;
  for(i=0;i<2;++i){
   *(void **)(entity[i]+0x190)=clients[i];actors[i].client=&sourceClients[i];
   *(int *)(clients[i]+0xb30)=sourceClients[i].sess.sessionTeam=i?flags&1:0;
   *(int *)(entity[i]+0x2c8)=actors[i].health=kind<2?100:-1;
   *(int *)(clients[i]+4)=sourceClients[i].ps.pm_type=kind==3?PM_DEAD:PM_NORMAL;
   *(int *)(entity[i]+8)=actors[i].s.eFlags=kind==4?EF_DEAD:0;
   *(int *)(clients[i]+0xc)=sourceClients[i].ps.pm_flags=flags&2?PMF_LIMBO:0;
   for(j=0;j<WS_MAX+1;++j){*(int *)(clients[i]+0xc14+j*20)=sourceClients[i].sess.aWeaponStats[j].atts=flags&1?5:0;}
  }
  if(kind==1){*(void **)(entity[0]+0x190)=NULL;actors[0].client=NULL;}
  if(attack==3){*(void **)(entity[1]+0x190)=NULL;actors[1].client=NULL;}
  stats(entity[0],attack==0?NULL:attack==1?entity[0]:entity[1],damage[dmg],mod);
  G_addStats(&actors[0],attack==0?NULL:attack==1?&actors[0]:&actors[1],damage[dmg],mod);
  if(!match(0)||!match(1)){printf("Stats mismatch state%d gt%d kind%d attacker%d flags%d mod%d dmg%d\n",state,gt,kind,attack,flags,mod,damage[dmg]);return 1;}++n;
 }
 printf("PASS %d complete G_addStats/G_weapStatIndex_MOD original comparisons\n",n);return 0;
}
