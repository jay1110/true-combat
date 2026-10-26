#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern void G_Voice(gentity_t*,gentity_t*,int,const char*,qboolean),G_VoiceTo(gentity_t*,gentity_t*,int,const char*,qboolean);
extern qboolean G_IsOnFireteam(int,fireteamData_t**);extern void BotRecordVoiceChat(int,int,const char*,int,qboolean);
static byte*base,rawClients[64][0x16b0];static gclient_t clients[64];static int side,v,count,bad;static int teams[2];
typedef struct{int k,a,b,c,d;char text[256];}event;static event calls[512];
typedef struct{int squelch,previous,count;event calls[512];}result;
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static byte*raw(int n){return base+0x226e2a0+n*0x57c;}static void*ent(int n){return side?(void*)&g_entities[n]:(void*)raw(n);}static int idx(void*p){return !p?-1:side?(gentity_t*)p-g_entities:((byte*)p-raw(0))/0x57c;}
static event*note(int k){event*e;if(count==512)exit(3);e=&calls[count++];e->k=k;return e;}
static int same(void*a,void*b){event*e=note(1);e->a=idx(a);e->b=idx(b);return(v>>8)&1;}
static int fireteam(int n,void**out){event*e=note(2);e->a=n;*out=&teams[(v>>10)&1?n%2:0];return (v>>9)&1;}
static void record(int a,int b,const char*s,int mode,int noResponse){event*e=note(3);e->a=a;e->b=b;e->c=mode;e->d=noResponse;Q_strncpyz(e->text,s,sizeof(e->text));}
static int location(void*p,char*out,int len){event*e=note(4);e->a=idx(p);e->b=len;Q_strncpyz(out,"Bridge [A]",len);return(v>>11)&1;}
static void send(int n,const char*s){event*e=note(5);e->a=n;Q_strncpyz(e->text,s,sizeof(e->text));}
static void print(const char*fmt,...){va_list a;event*e=note(6);va_start(a,fmt);Q_vsnprintf(e->text,sizeof(e->text),fmt,a);va_end(a);}
static void argument(int n,char*out,int len){event*e=note(7);int counts[]={-1,0,1,3,64,65};e->a=n;e->b=len;if(n==1)Com_sprintf(out,len,"%d",(v/7)%7-1);else if(n==2)Com_sprintf(out,len,"%d",counts[(v/49)%6]);else Com_sprintf(out,len,"%d",(n*17+v)%68-2);}
static void capture(result*r){memset(r,0,sizeof(*r));r->squelch=side?g_entities[5].voiceChatSquelch:*(int*)(raw(5)+0x470);r->previous=side?g_entities[5].voiceChatPreviousTime:*(int*)(raw(5)+0x474);r->count=count;memcpy(r->calls,calls,sizeof(calls));}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*voice)(void*,void*,int,const char*,int),(__cdecl*to)(void*,void*,int,const char*,int);int phase,i,j,n,total=0,target,mode;unsigned r;static result a,b;int squelch[]={-1,0,29999,30000,30001,45000},allowed[]={-1,0,1,2,4,100};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;voice=(void*)(base+0x51820);to=(void*)(base+0x51530);
#define H(r,f,t) if(!hook(base+r,t)||!hook(f,t))return 2
 H(0x8daa0,OnSameTeam,same);H(0x5c110,G_IsOnFireteam,fireteam);H(0x280c0,BotRecordVoiceChat,record);H(0x8ffc0,Team_GetLocationMsg,location);H(0x8ad60,trap_SendServerCommand,send);H(0x5ed80,G_Printf,print);H(0x8ab70,trap_Argv,argument);
 for(phase=0;phase<2;phase++)for(v=0;v<32768;v++)for(side=0;side<2;side++){
  count=bad=0;memset(calls,0,sizeof(calls));memset(clients,0,sizeof(clients));memset(rawClients,0,sizeof(rawClients));level.clients=clients;*(void**)(base+0x7feb80)=rawClients;level.time=*(int*)(base+0x7febe0)=100000;r=(unsigned)v*1664525u+1013904223u;level.numConnectedClients=*(int*)(base+0x7fec0c)=(r>>17)%65;g_voiceChatsAllowed.integer=*(int*)(base+0x7e1f6c)=allowed[(r>>7)%6];g_dedicated.integer=*(int*)(base+0x23cefec)=(r>>4)&1;
  for(i=0;i<64;i++){gentity_t*e=&g_entities[i];byte*p=raw(i);memset(e,0,sizeof(*e));memset(p,0,0x57c);e->client=&clients[i];*(void**)(p+0x190)=rawClients[i];e->inuse=*(int*)(p+0x194)=1;e->s.number=*(int*)p=63-i;e->s.clientNum=*(int*)(p+0xac)=i;clients[i].ps.clientNum=*(int*)(rawClients[i]+0xa0)=42;clients[i].sess.sessionTeam=*(int*)(rawClients[i]+0xb30)=(v/16+i)%4;clients[i].ps.stats[STAT_HEALTH]=*(int*)(rawClients[i]+0xd0)=(v/64+i)%3-1;clients[i].sess.playerType=*(int*)(rawClients[i]+0xb40)=i%5;strcpy(clients[i].pers.netname,"Speaker");strcpy((char*)rawClients[i]+0x5f8,"Speaker");level.sortedClients[i]=*(int*)(base+0x7fec18+i*4)=(i*17+v)%64;for(j=0;j<3;j++)e->s.pos.trBase[j]=*(float*)(p+0x18+j*4)=(i*73+j*13-500)*.33333334f;}
  g_entities[5].voiceChatSquelch=*(int*)(raw(5)+0x470)=squelch[(r>>20)%6];g_entities[5].voiceChatPreviousTime=*(int*)(raw(5)+0x474)=100000+(int)((r>>23)%3)-1;n=(v/4)%4;target=n?9:-1;if(n==1)g_entities[9].inuse=*(int*)(raw(9)+0x194)=0;if(n==2){g_entities[9].client=NULL;*(void**)(raw(9)+0x190)=NULL;}if(v&0x1000)g_entities[9].s.clientNum=*(int*)(raw(9)+0xac)=5;mode=v%4;
  if(!phase){if(side)G_VoiceTo(ent(5),target<0?NULL:ent(target),mode,"NeedBackup",(v/192)%3);else to(ent(5),target<0?NULL:ent(target),mode,"NeedBackup",(v/192)%3);}else{if(side)G_Voice(ent(5),target<0?NULL:ent(target),mode,"NeedBackup",(v/192)%3);else voice(ent(5),target<0?NULL:ent(target),mode,"NeedBackup",(v/192)%3);}capture(side?&b:&a);
  if(side){if(bad||memcmp(&a,&b,sizeof(a))){printf("Server voice mismatch phase%d case%d calls%d/%d squelch%d/%d\n",phase,v,a.count,b.count,a.squelch,b.squelch);for(i=0;i<a.count||i<b.count;i++)if(memcmp(&a.calls[i],&b.calls[i],sizeof(event)))printf("call%d %d,%d,%d,%d,%d[%s] / %d,%d,%d,%d,%d[%s]\n",i,a.calls[i].k,a.calls[i].a,a.calls[i].b,a.calls[i].c,a.calls[i].d,a.calls[i].text,b.calls[i].k,b.calls[i].a,b.calls[i].b,b.calls[i].c,b.calls[i].d,b.calls[i].text);return 1;}total++;}
 }
 printf("PASS whole G_VoiceTo and connected G_Voice %d cases (32768 each)\n",total);return 0;
}
