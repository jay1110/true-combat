#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
extern void ClientTimerActions(gentity_t*,int);
extern short TCE_FindClosestNodeToPoint(gentity_t*,const vec3_t);
extern void TCE_ShowNode(int,int);
extern vmCvar_t bot_editWaypoints;
static byte*moduleBase;static byte rawPlayerClient[0x1800];static gclient_t playerClient;static int nearCalls;

static byte rawEnt[0x600],rawClient[0x1800],rawLocation[0x600];static gentity_t ent,location;static gclient_t client;
static char trace[2048];static int original,locationMode,ticks;static void*activeEntity;
static gentity_t* findLocation(gentity_t*e){assert((void*)e==activeEntity);strcat(trace,"L;");++ticks;if(!locationMode||(locationMode==2&&ticks%2==0))return NULL;return original?(gentity_t*)rawLocation:&location;}
static void damage(gentity_t*t,gentity_t*i,gentity_t*a,vec3_t dir,vec3_t point,int amount,int flags,int mod){assert((void*)t==activeEntity&&i==t&&a==t);assert(dir[0]==1&&dir[1]==0&&dir[2]==0);assert((void*)point==(original?(void*)(rawClient+0x14):(void*)client.ps.origin));assert(amount==1&&flags==0&&mod==9);strcat(trace,"D;");}
static void logvec(const float*p){sprintf(trace+strlen(trace),"%08x,%08x,%08x;",*(unsigned*)(p+0),*(unsigned*)(p+1),*(unsigned*)(p+2));}
static short nearest(gentity_t*e,const vec3_t p){assert((void*)e==(original?(void*)(moduleBase+0x226e2a0):(void*)&g_entities[0]));strcat(trace,"N:");logvec(p);return nearCalls++%2?(short)-1:(short)321;}
static void show(int node,int type){sprintf(trace+strlen(trace),"S%d,%d;",node,type);}
static void ray(trace_t*out,const vec3_t start,const vec3_t mins,const vec3_t maxs,const vec3_t end,int pass,int mask){assert(!mins&&!maxs&&pass==19&&mask==1);strcat(trace,"R:");logvec(start);logvec(end);memset(out,0,sizeof(*out));VectorSet(out->endpos,1.13f,-71.37f,99.71f);out->fraction=.5f;}
static struct{unsigned char*p,bytes[5];}hooks[10];static int hc;
static void patch(void*a,void*f){unsigned char*p=a;DWORD o,t;assert(hc<10);hooks[hc].p=p;memcpy(hooks[hc++].bytes,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].bytes,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}


int main(int argc,char**argv){HMODULE h;byte*b;int r,m,d,bot,l,cn,ang,cases=0;int residuals[]={-1,0,1,999,1000,2500},deltas[]={0,1,16,999,1000,1001,2000,3333},debugs[]={-1,0,3,4,9};char expected[2048];int er,el;
assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;moduleBase=b;patch(b+0x589c0,damage);patch(G_Damage,damage);patch(b+0x8ff00,findLocation);patch(Team_GetLocation,findLocation);patch(b+0x41450,nearest);patch(TCE_FindClosestNodeToPoint,nearest);patch(b+0x41760,show);patch(TCE_ShowNode,show);patch(b+0x8ae80,ray);patch(trap_Trace,ray);
for(r=0;r<6;++r)for(m=0;m<8;++m)for(d=0;d<5;++d)for(bot=0;bot<5;++bot)for(l=0;l<3;++l)for(cn=0;cn<2;++cn)for(ang=0;ang<3;++ang){memset(&client,0,sizeof(client));memset(rawClient,0,sizeof(rawClient));ent.client=&client;*(void**)(rawEnt+400)=rawClient;ent.health=*(int*)(rawEnt+0x2c8)=65;client.sess.playerType=PC_MEDIC;client.ps.stats[STAT_MAX_HEALTH]=100;client.timeResidual=*(int*)(rawClient+0xe74)=residuals[r];client.ps.holdable[13]=*(int*)(rawClient+0x3c4)=73;location.health=*(int*)(rawLocation+0x2c8)=27;g_debugBullets.integer=*(int*)(b+0x7849cc)=debugs[d];bot_editWaypoints.integer=*(int*)(b+0x6d988c)=bot;locationMode=l;
client.ps.clientNum=*(int*)(rawClient+0xa0)=cn;g_entities[0].client=&playerClient;*(void**)(b+0x226e2a0+0x190)=rawPlayerClient;g_entities[0].s.number=*(int*)(b+0x226e2a0)=19;
VectorSet(g_entities[0].r.currentOrigin,13.17f,-9.71f,23.13f);memcpy(b+0x226e2a0+0x168,g_entities[0].r.currentOrigin,12);VectorSet(playerClient.ps.viewangles,ang?23.17f:0,ang==2?-123.71f:ang?71.13f:0,0);memcpy(rawPlayerClient+0xb0,playerClient.ps.viewangles,12);playerClient.ps.viewheight=*(int*)(rawPlayerClient+0xbc)=47;

original=1;activeEntity=rawEnt;trace[0]=0;ticks=0;nearCalls=0;((void(__cdecl*)(void*,int))(b+0x45960))(rawEnt,deltas[m]);strcpy(expected,trace);er=*(int*)(rawClient+0xe74);el=*(int*)(rawClient+0x3c4);original=0;activeEntity=&ent;trace[0]=0;ticks=0;nearCalls=0;ClientTimerActions(&ent,deltas[m]);if(strcmp(expected,trace)||er!=client.timeResidual||el!=client.ps.holdable[13]||ent.health!=65){fprintf(stderr,"timer whole mismatch r%d m%d d%d bot%d l%d\nO%s\nS%s\n",r,m,d,bot,l,expected,trace);return 1;}++cases;}
restore();assert(FreeLibrary(h));printf("%d whole timer/editor/damage/location comparisons passed\n",cases);return 0;}
