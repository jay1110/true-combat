#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
static byte*base,rawClient[0x16b0];static gclient_t client;static int side,count,bad,variant;
typedef struct{int k,a,b,c,flags,lives;char text[128];}event;static event calls[8];
typedef struct{int flags,persist[16],saved[16],hostage[4],count;event calls[8];}result;
static const char*settings[]={"","\\respawn\\no","\\respawn\\off","\\respawn\\NO","\\respawn\\OFF","\\respawn\\yes","\\respawn\\0","\\respawn\\false","\\respawn\\on","\\respawn\\ no","\\respawn\\officer","\\name\\bob"};
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static void*entity(void){return side?(void*)&g_entities[5]:(void*)(base+0x226e2a0+5*0x57c);}
static event*note(int k){event*e;if(count>=8)exit(3);e=&calls[count++];e->k=k;e->flags=side?client.ps.pm_flags:*(int*)(rawClient+0xc);e->lives=side?client.ps.persistant[PERS_RESPAWNS_LEFT]:*(int*)(rawClient+0x12c);return e;}
static void userinfo(int n,char*out,int len){event*e=note(1);e->a=n;e->b=len;Q_strncpyz(out,settings[variant%12],len);}
static void removeClients(void*p,int team){event*e=note(2);e->a=team;if(p!=entity())bad=1;/* Real callee boundary may update saved state: copy must follow both calls. */if(variant&0x100){if(side)client.saved_persistant[team]+=team*7;else*(int*)(rawClient+0xfac+team*4)+=team*7;}}
static void spawn(void*p,int revive,int hostage){event*e=note(3);e->a=revive;e->b=hostage;if(p!=entity())bad=1;}
static void print(const char*fmt,...){va_list a;event*e=note(4);va_start(a,fmt);Q_vsnprintf(e->text,sizeof(e->text),fmt,a);va_end(a);}
static void debug(const char*fmt,...){va_list a;event*e=note(5);va_start(a,fmt);Q_vsnprintf(e->text,sizeof(e->text),fmt,a);va_end(a);}
static void capture(result*r){int i;memset(r,0,sizeof(*r));r->flags=side?client.ps.pm_flags:*(int*)(rawClient+0xc);for(i=0;i<16;i++){r->persist[i]=side?client.ps.persistant[i]:*(int*)(rawClient+0x110+i*4);r->saved[i]=side?client.saved_persistant[i]:*(int*)(rawClient+0xfac+i*4);}r->hostage[0]=side?level.tceHostageTeam:*(int*)(base+0x222394c);r->hostage[1]=side?level.tceHostageActive:*(int*)(base+0x2223950);r->hostage[2]=side?level.tceHostageSecured:*(int*)(base+0x2223954);r->hostage[3]=side?level.tceHostageRespawnCount:*(int*)(base+0x2223958);r->count=count;memcpy(r->calls,calls,sizeof(calls));}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*originalRespawn)(void*,int),(__cdecl*originalReinforce)(void*,int);int phase,v,i,hostage,total=0;unsigned r;static result a,b;int types[]={2,5,7},lives[]={-1,0,1,2,99};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;originalRespawn=(void*)(base+0x4b760);originalReinforce=(void*)(base+0x4b680);
#define H(r,f,t) if(!hook(base+r,t)||!hook(f,t))return 2
 H(0x8ae00,trap_GetUserinfo,userinfo);H(0x75e30,G_smvRemoveInvalidClients,removeClients);H(0x4cd90,ClientSpawn,spawn);H(0x5ed80,G_Printf,print);H(0x5edc0,G_DPrintf,debug);
 for(phase=0;phase<2;phase++)for(v=0;v<65536;v++)for(side=0;side<2;side++){
  byte*p=base+0x226e2a0+5*0x57c;gentity_t*e=&g_entities[5];variant=v;count=bad=0;memset(calls,0,sizeof(calls));memset(&client,0,sizeof(client));memset(rawClient,0,sizeof(rawClient));memset(e,0,sizeof(*e));memset(p,0,0x57c);e->client=&client;*(void**)(p+0x190)=rawClient;e->s.number=*(int*)p=5;e->r.svFlags=*(int*)(p+0x128)=(((unsigned)v*1664525u+1013904223u)>>28)&1?SVF_BOT:0;hostage=(v/2)%3;
  r=(unsigned)v*1664525u+1013904223u;
#define C(field,off,x) client.field=*(int*)(rawClient+off)=(x)
#define L(field,off,x) level.field=*(int*)(base+off)=(x)
#define CV(field,off,x) field.integer=*(int*)(base+off)=(x)
  C(ps.pm_flags,0xc,((v/6)%2?PMF_LIMBO:0)|0x2000|0x80);C(sess.sessionTeam,0xb30,(v/12)%4);C(pers.mvCount,0xae8,(v/48)%3-1);
  for(i=0;i<16;i++){C(ps.persistant[i],0x110+i*4,100+i);C(saved_persistant[i],0xfac+i*4,200+i);}
  C(ps.persistant[PERS_RESPAWNS_LEFT],0x12c,lives[(v/144)%5]);C(saved_persistant[PERS_RESPAWNS_LEFT],0xfac+PERS_RESPAWNS_LEFT*4,lives[(v/720)%5]);Q_strncpyz(client.pers.netname,"Lifecycle",sizeof(client.pers.netname));strcpy((char*)rawClient+0x5f8,"Lifecycle");
  CV(g_gametype,0x6d8d4c,types[(v/3600)%3]);CV(g_gamestate,0x784f6c,(r>>4)%4);CV(g_maxlives,0x6d7fcc,(int)((r>>6)%3)-1);CV(g_alliedmaxlives,0x6d91cc,(int)((r>>8)%3)-1);CV(g_axismaxlives,0x23ceecc,(int)((r>>10)%3)-1);
  L(tceHostageTeam,0x222394c,(r>>12)%3);L(tceHostageActive,0x2223950,(r>>14)%2);L(tceHostageSecured,0x2223954,(r>>15)%2);L(tceHostageRespawnCount,0x2223958,(int)((r>>16)%6)-1);L(numFinalDead[0],0x221a418,(r>>20)%5);L(numFinalDead[1],0x221a41c,(r>>24)%5);
  if(!phase){if(side)respawn(entity(),hostage);else originalRespawn(entity(),hostage);}else{if(side)reinforce(entity(),hostage);else originalReinforce(entity(),hostage);}capture(side?&b:&a);
  if(side){if(bad||memcmp(&a,&b,sizeof(a))){printf("Reinforcement lifecycle mismatch phase%d case%d bad%d calls%d/%d flags%x/%x\n",phase,v,bad,a.count,b.count,a.flags,b.flags);for(i=0;i<sizeof(a);i++)if(((byte*)&a)[i]!=((byte*)&b)[i]){printf("byte%x %02x/%02x\n",i,((byte*)&a)[i],((byte*)&b)[i]);break;}for(i=0;i<a.count||i<b.count;i++)if(memcmp(&a.calls[i],&b.calls[i],sizeof(event)))printf("call%d kind%d/%d args%d,%d/%d,%d text[%s]/[%s]\n",i,a.calls[i].k,b.calls[i].k,a.calls[i].a,a.calls[i].b,b.calls[i].a,b.calls[i].b,a.calls[i].text,b.calls[i].text);return 1;}total++;}
 }
 printf("PASS whole respawn/reinforce %d cases (65536 each), real reinforce-to-respawn chain\n",total);return 0;
}
