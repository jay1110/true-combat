#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern int Pickup_Health(gentity_t *,gentity_t *);
static gentity_t actor,itemEntity;static gclient_t client;static gitem_t item;
static byte raw[0x57c],rawItem[0x57c],rawClient[0x2000],rawDef[60];
int main(int argc,char**argv){HMODULE dll;byte*b;int(__cdecl*can)(int,void*);int(__cdecl*health)(void*,void*);int gt,t,c,s,w,k,j,n=0,h=0;
 const int skill[]={-1,0,3,4,5};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;can=(void*)(b+0x5d450);health=(void*)(b+0x5da60);
 actor.client=&client;itemEntity.item=&item;*(void**)(raw+0x190)=rawClient;*(void**)(rawItem+0x33c)=rawDef;
 for(gt=-1;gt<=9;++gt)for(t=0;t<4;++t)for(c=0;c<7;++c)for(s=0;s<5;++s)for(w=-16;w<=80;++w){
  g_gametype.integer=*(int*)(b+0x6d8d4c)=gt;client.sess.sessionTeam=*(int*)(rawClient+0xb30)=t;
  client.sess.playerType=*(int*)(rawClient+0xb40)=c;client.sess.skill[SK_HEAVY_WEAPONS]=*(int*)(rawClient+0xbb8)=skill[s];
  if(can(w,raw)!=G_CanPickupWeapon(w,&actor)){fprintf(stderr,"pickup policy gt%d team%d class%d skill%d w%d mismatch\n",gt,t,c,skill[s],w);return 1;}++n;
 }
 for(k=0;k<100000;++k){int x,y,total=0;
  for(j=2;j<=4;++j){int value=(k*(j*17+3)+j)%121;client.ps.holdable[j]=value;*(int*)(rawClient+0x390+j*4)=value;total+=value;}
  if(!total)continue; /* Original 0/0 conversion is undefined; separately documented repair. */
  item.quantity=*(int*)(rawDef+0x20)=k%201-50;
  client.ps.stats[STAT_MAX_HEALTH]=*(int*)(rawClient+0xe0)=k%151;
  actor.health=*(int*)(raw+0x2c8)=-17;
  x=health(rawItem,raw);y=Pickup_Health(&itemEntity,&actor);
  if(x!=y||actor.health!=*(int*)(raw+0x2c8)||client.ps.stats[STAT_HEALTH]!=*(int*)(rawClient+0xd0)||memcmp(client.ps.holdable+2,rawClient+0x398,12)){fprintf(stderr,"health pickup case%d mismatch health%d/%d\n",k,actor.health,*(int*)(raw+0x2c8));return 1;}++h;
 }
 printf("%d whole pickup policies and %d defined health pickups matched\n",n,h);return 0;
}
