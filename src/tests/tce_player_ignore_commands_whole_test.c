#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern void Cmd_Ignore_f(gentity_t*),Cmd_UnIgnore_f(gentity_t*);
static byte*base,rawClient[0x16b0];static gclient_t client;static int side,v,count,bad,slot,resolved;static char argument[1024];
typedef struct{int k,a,b;char text[1100];}event;static event calls[4];
typedef struct{unsigned mask[2];int count;event calls[4];}result;
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static event*note(int k){event*e;if(count==4)exit(3);e=&calls[count++];e->k=k;return e;}
static void argv(int n,char*out,int len){event*e=note(1);e->a=n;e->b=len;Q_strncpyz(out,argument,len);}
static int resolve(void*p,const char*s){event*e=note(2);e->a=side?(gentity_t*)p-g_entities:((byte*)p-base-0x226e2a0)/0x57c;Q_strncpyz(e->text,s,sizeof(e->text));return resolved;}
static void send(int n,const char*s){event*e=note(3);e->a=n;Q_strncpyz(e->text,s,sizeof(e->text));}
int main(int argc,char**argvInput){HMODULE dll;void(__cdecl*ignore)(void*),(__cdecl*unignore)(void*);int phase,i,total=0;static result a,b;unsigned masks[]={0,0xffffffffu,0xaaaaaaaau,0x55555555u,0x80000001u};
 if(argc!=2||!(dll=LoadLibraryA(argvInput[1])))return 2;base=(byte*)dll;ignore=(void*)(base+0x54900);unignore=(void*)(base+0x54990);
#define H(r,f,t) if(!hook(base+r,t)||!hook(f,t))return 2
 H(0x8ab70,trap_Argv,argv);H(0x7c680,G_refClientnumForName,resolve);H(0x8ad60,trap_SendServerCommand,send);
 for(phase=0;phase<2;phase++)for(v=0;v<10400;v++)for(side=0;side<2;side++){
  byte*p;gentity_t*e;result*r=side?&b:&a;slot=(v/65)%64;resolved=v%65;p=base+0x226e2a0+slot*0x57c;e=&g_entities[slot];memset(e,0,sizeof(*e));memset(p,0,0x57c);memset(&client,0,sizeof(client));memset(rawClient,0,sizeof(rawClient));e->client=&client;*(void**)(p+0x190)=rawClient;e->s.number=*(int*)p=63-slot;client.ps.clientNum=*(int*)(rawClient+0xa0)=17;count=bad=0;memset(calls,0,sizeof(calls));
  for(i=0;i<2;i++)client.sess.ignoreClients[i]=*(unsigned*)(rawClient+0xb5c+i*4)=masks[(v/130+i)%5];memset(argument,'X',sizeof(argument));argument[1023]=0;if((v/650)%4==0)argument[0]=0;else if((v/650)%4==1)strcpy(argument,"^1Player Name");else if((v/650)%4==2)strcpy(argument,"31");
  if(!phase){if(side)Cmd_Ignore_f(e);else ignore(p);}else{if(side)Cmd_UnIgnore_f(e);else unignore(p);}memset(r,0,sizeof(*r));memcpy(r->mask,side?(void*)client.sess.ignoreClients:(void*)(rawClient+0xb5c),8);r->count=count;memcpy(r->calls,calls,sizeof(calls));
  if(side){if(bad||memcmp(&a,&b,sizeof(a))){printf("Player ignore mismatch phase%d case%d mask%x,%x/%x,%x calls%d/%d\n",phase,v,a.mask[0],a.mask[1],b.mask[0],b.mask[1],a.count,b.count);for(i=0;i<a.count||i<b.count;i++)if(memcmp(&a.calls[i],&b.calls[i],sizeof(event)))printf("call%d %d,%d,%d[%s] / %d,%d,%d[%s]\n",i,a.calls[i].k,a.calls[i].a,a.calls[i].b,a.calls[i].text,b.calls[i].k,b.calls[i].a,b.calls[i].b,b.calls[i].text);return 1;}total++;}
 }
 printf("PASS whole Ignore/Unignore commands %d cases (10400 each), real bitset helpers\n",total);return 0;
}
