#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern int G_ComputeMaxLives(gclient_t*,int);
static byte*base,clients[0x16b0*4];static gclient_t sc[4];static int side,variant,number,bad,count;
typedef struct{int k,a;char text[256];}event;static event events[40];
static void*ent(void){return side?(void*)&g_entities[number]:(void*)(base+0x226e2a0+number*0x57c);}
static void*cl(void){return side?(void*)&sc[number]:(void*)(clients+number*0x16b0);}
static void note(int k,int a,const char*s){event*e;if(count>=40)exit(3);e=&events[count++];memset(e,0,sizeof(*e));e->k=k;e->a=a;if(s)Q_strncpyz(e->text,s,sizeof(e->text));}
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static void check(void*p){if(p!=ent())bad=1;}
static void unlinker(void*p){check(p);note(1,number,NULL);}
static void init(void*p){check(p);note(2,number,NULL);}
static void spawn(void*p,int revived,int hostage){check(p);note(3,revived, NULL);if(hostage)bad=1;if(side){if(sc[number].ps.eFlags!=0x1234||sc[number].ps.persistant[PERS_SPAWN_COUNT]!=9||sc[number].ps.persistant[PERS_RESPAWNS_LEFT]!=variant%9-3||sc[number].pers.connected!=CON_CONNECTED||sc[number].pers.complaintClient!=-1)bad=1;sc[number].ps.stats[STAT_HEALTH]=81;if(variant&32)sc[number].sess.sessionTeam=TEAM_SPECTATOR;}else{byte*c=cl();if(*(int*)(c+0x68)!=0x1234||*(int*)(c+0x120)!=9||*(int*)(c+0x12c)!=variant%9-3||*(int*)(c+0x5ac)!=2||*(int*)(c+0x66c)!=-1)bad=1;*(int*)(c+0xd0)=81;if(variant&32)*(int*)(c+0xb30)=3;}}
static void markers(void*p){check(p);note(4,0,NULL);}
static int lives(void*p,int n){if(p!=cl())bad=1;note(5,n,NULL);return n-2;}
static void limboHook(void*p,int corpse){check(p);note(6,corpse,NULL);if(variant&64){if(side)sc[number].sess.sessionTeam=3;else*(int*)((byte*)cl()+0xb30)=3;}}
static void send(int n,const char*s){note(7,n,s);}
static void QDECL logmsg(const char*f,...){va_list a;char b[256];va_start(a,f);Q_vsnprintf(b,sizeof(b),f,a);va_end(a);note(8,0,b);}
static void user(int n,char*b,int len){note(9,n,NULL);if(len!=1024)bad=1;Q_strncpyz(b,"\\cl_guid\\GUID123\\ip\\127.0.0.1:27960",len);}
static void guid(char*s){note(10,0,s);}static void ban(char*s){note(11,0,s);}static void ranks(void){note(12,0,NULL);}static void smv(void*p){check(p);note(13,0,NULL);}
typedef struct{int n,eflags,spawn,lives,pm,health,team,connected,complaint,end,calced,backup,defuse,enthealth,contents,surface,client,touch,pain;event log[40];}result;
int main(int argc,char**argv){HMODULE dll;void(__cdecl*original)(int);int gm,team,v,k,n=0;int modes[]={2,5,7};int times[]={599,600,601,10000};result a,b;gentity_t*e;gclient_t*c;byte*r,*q;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;original=(void*)(base+0x4ca70);level.clients=sc;*(void**)(base+0x7feb80)=clients;
#define HOOK(rva,fn,repl) if(!hook(base+rva,repl)||!hook(fn,repl))return 2
 HOOK(0x8afc0,trap_UnlinkEntity,unlinker);HOOK(0x94da0,G_InitGentity,init);HOOK(0x4cd90,ClientSpawn,spawn);HOOK(0x4e3a0,G_ResetMarkers,markers);HOOK(0x4ca00,G_ComputeMaxLives,lives);HOOK(0x4b410,limbo,limboHook);HOOK(0x8ad60,trap_SendServerCommand,send);HOOK(0x62600,G_LogPrintf,logmsg);HOOK(0x8ae00,trap_GetUserinfo,user);HOOK(0x88ab0,AddMaxLivesGUID,guid);HOOK(0x88a90,AddMaxLivesBan,ban);HOOK(0x61b60,CalculateRanks,ranks);HOOK(0x75e10,G_smvUpdateClientCSList,smv);
 for(gm=0;gm<3;gm++)for(team=0;team<4;team++)for(v=0;v<1024;v++)for(side=0;side<2;side++){variant=v;number=v%4;bad=count=0;memset(events,0,sizeof(events));e=&g_entities[number];c=&sc[number];r=base+0x226e2a0+number*0x57c;q=clients+number*0x16b0;memset(e,0,sizeof(*e));memset(c,0,sizeof(*c));memset(r,0,0x57c);memset(q,0,0x16b0);memset(&b,0,sizeof(b));
#define CVAR(field,rva,val) field.integer=*(int*)(base+rva)=val
 CVAR(g_gametype,0x6d8d4c,modes[gm]);CVAR(g_maxlives,0x6d7fcc,v%3-1);CVAR(g_axismaxlives,0x23ceecc,(v/3)%3-1);CVAR(g_alliedmaxlives,0x6d91cc,(v/9)%3-1);CVAR(g_enforcemaxlives,0x23ce92c,(v/27)%3);
 level.startTime=*(int*)(base+0x7febec)=10000;level.time=*(int*)(base+0x7febe0)=10000+times[(v/81)%4];e->r.linked=*(int*)(r+0x120)=v&1;e->health=*(int*)(r+0x2c8)=91;e->surfaceFlags=*(int*)(r+0x534)=123;
 c->ps.eFlags=*(int*)(q+0x68)=0x1234;c->ps.persistant[PERS_SPAWN_COUNT]=*(int*)(q+0x120)=9;c->ps.persistant[PERS_RESPAWNS_LEFT]=*(int*)(q+0x12c)=v%9-2;c->sess.sessionTeam=*(int*)(q+0xb30)=team;c->maxlivescalced=*(int*)(q+0x12f8)=v&2;c->backupMarker.serverTime=*(int*)(q+0x16a0)=99;c->tceDefuseActive=*(int*)(q+0x130c)=1;strcpy(c->pers.netname,"Player");strcpy((char*)q+0x5f8,"Player");
 if(side)ClientBegin(number);else original(number);
#define C(name,field,off) b.name=side?c->field:*(int*)(q+off)
 C(eflags,ps.eFlags,0x68);C(spawn,ps.persistant[PERS_SPAWN_COUNT],0x120);C(lives,ps.persistant[PERS_RESPAWNS_LEFT],0x12c);C(pm,ps.pm_type,4);C(health,ps.stats[STAT_HEALTH],0xd0);C(team,sess.sessionTeam,0xb30);C(connected,pers.connected,0x5ac);C(complaint,pers.complaintClient,0x66c);C(end,pers.complaintEndTime,0x670);C(calced,maxlivescalced,0x12f8);C(backup,backupMarker.serverTime,0x16a0);C(defuse,tceDefuseActive,0x130c);
 b.enthealth=side?e->health:*(int*)(r+0x2c8);b.contents=side?e->r.contents:*(int*)(r+0x14c);b.surface=side?e->surfaceFlags:*(int*)(r+0x534);b.client=side?(e->client==c):(*(void**)(r+0x190)==q);b.touch=side?(e->touch!=NULL):(*(void**)(r+0x2b0)!=NULL);b.pain=side?(e->pain!=NULL):(*(void**)(r+0x2b8)!=NULL);b.n=count;memcpy(b.log,events,sizeof(events));
 if(!side)a=b;else{if(bad||memcmp(&a,&b,sizeof(a))){printf("ClientBegin mismatch gm%d team%d v%d bad%d events%d/%d\n",modes[gm],team,v,bad,a.n,b.n);for(k=0;k<a.n||k<b.n;k++)printf("%d: %d %d [%s] / %d %d [%s]\n",k,a.log[k].k,a.log[k].a,a.log[k].text,b.log[k].k,b.log[k].a,b.log[k].text);return 1;}n++;}}
 printf("PASS ClientBegin %d whole cases\n",n);return 0;
}
