#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
extern void ClientTimerActions(gentity_t*,int);
static byte rawEnt[0x600],rawClient[0x1800],rawLocation[0x600];static gentity_t ent,location;static gclient_t client;
static char trace[2048];static int original,locationMode,ticks;static void*activeEntity;
static gentity_t* findLocation(gentity_t*e){assert((void*)e==activeEntity);strcat(trace,"L;");++ticks;if(!locationMode||(locationMode==2&&ticks%2==0))return NULL;return original?(gentity_t*)rawLocation:&location;}
static void damage(gentity_t*t,gentity_t*i,gentity_t*a,vec3_t dir,vec3_t point,int amount,int flags,int mod){assert((void*)t==activeEntity&&i==t&&a==t);assert(dir[0]==1&&dir[1]==0&&dir[2]==0);assert((void*)point==(original?(void*)(rawClient+0x14):(void*)client.ps.origin));assert(amount==1&&flags==0&&mod==9);strcat(trace,"D;");}
static struct{unsigned char*p,bytes[5];}hooks[4];static int hc;
static void patch(void*a,void*f){unsigned char*p=a;DWORD o,t;assert(hc<4);hooks[hc].p=p;memcpy(hooks[hc++].bytes,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].bytes,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}


int main(int argc,char**argv){HMODULE h;byte*b;int r,m,d,bot,l,cases=0;int residuals[]={-1,0,1,999,1000,2500},deltas[]={0,1,16,999,1000,1001,2000,3333},debugs[]={-1,0,3,4,9};char expected[2048];int er,el;
assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;patch(b+0x589c0,damage);patch(G_Damage,damage);patch(b+0x8ff00,findLocation);patch(Team_GetLocation,findLocation);
for(r=0;r<6;++r)for(m=0;m<8;++m)for(d=0;d<5;++d)for(bot=0;bot<2;++bot)for(l=0;l<3;++l){memset(&client,0,sizeof(client));memset(rawClient,0,sizeof(rawClient));ent.client=&client;*(void**)(rawEnt+400)=rawClient;ent.health=*(int*)(rawEnt+0x2c8)=65;client.sess.playerType=PC_MEDIC;client.ps.stats[STAT_MAX_HEALTH]=100;client.timeResidual=*(int*)(rawClient+0xe74)=residuals[r];client.ps.holdable[13]=*(int*)(rawClient+0x3c4)=73;location.health=*(int*)(rawLocation+0x2c8)=27;g_debugBullets.integer=*(int*)(b+0x7849cc)=debugs[d];*(int*)(b+0x6d988c)=bot;locationMode=l;
original=1;activeEntity=rawEnt;trace[0]=0;ticks=0;((void(__cdecl*)(void*,int))(b+0x45960))(rawEnt,deltas[m]);strcpy(expected,trace);er=*(int*)(rawClient+0xe74);el=*(int*)(rawClient+0x3c4);original=0;activeEntity=&ent;trace[0]=0;ticks=0;ClientTimerActions(&ent,deltas[m]);if(strcmp(expected,trace)||er!=client.timeResidual||el!=client.ps.holdable[13]||ent.health!=65){fprintf(stderr,"timer partial mismatch r%d m%d d%d bot%d l%d\nO%s\nS%s\n",r,m,d,bot,l,expected,trace);return 1;}++cases;}
restore();assert(FreeLibrary(h));printf("%d partial timer/damage/location comparisons passed (bot debug excluded)\n",cases);return 0;}
