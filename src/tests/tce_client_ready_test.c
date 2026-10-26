#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern void Cmd_IntermissionReady_f(gentity_t *ent);
extern void G_SetClientSound(gentity_t *ent);
static byte raw[0x57c],rawBefore[0x57c],rawClient[0x2000],rawClientBefore[0x2000];
static gentity_t entity,entityBefore;
static gclient_t client,clientBefore;
static void setup(int scenario){
 int i;for(i=0;i<sizeof(raw);i++)raw[i]=(byte)(scenario+i*31);
 for(i=0;i<sizeof(rawClient);i++)rawClient[i]=(byte)(scenario*7+i*17);
 memset(&entity,0x5a,sizeof(entity));memset(&client,0xa5,sizeof(client));
 entity.client=&client;*(void**)(raw+0x190)=rawClient;
 entity.s.eFlags=*(int*)(raw+8)=scenario*7919;
 client.ps.eFlags=*(int*)(rawClient+0x68)=~(scenario*13);
 client.pers.ready=*(int*)(rawClient+0xafc)=scenario%5-2;
 entity.s.loopSound=*(int*)(raw+0xa0)=scenario*31-100;
}
static void snapshot(void){memcpy(rawBefore,raw,sizeof(raw));memcpy(rawClientBefore,rawClient,sizeof(rawClient));entityBefore=entity;clientBefore=client;}
static int unchanged(void){return !memcmp(rawBefore,raw,sizeof(raw))&&!memcmp(rawClientBefore,rawClient,sizeof(rawClient))&&!memcmp(&entityBefore,&entity,sizeof(entity))&&!memcmp(&clientBefore,&client,sizeof(client));}
int main(int argc,char **argv){
 HMODULE dll;byte*b;void(__cdecl*ready)(void*);void(__cdecl*command)(void*);void(__cdecl*sound)(void*);int mode,i,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;ready=(void*)(b+0x543a0);command=(void*)(b+0x54420);sound=(void*)(b+0x44af0);
 for(mode=0;mode<2;mode++)for(i=0;i<4096;i++){
  setup(i);snapshot();if(mode){command(raw);Cmd_IntermissionReady_f(&entity);}else{ready(raw);G_MakeReady(&entity);}
  if(entity.s.eFlags!=*(int*)(raw+8)||client.ps.eFlags!=*(int*)(rawClient+0x68)||client.pers.ready!=*(int*)(rawClient+0xafc)){fprintf(stderr,"ready mode%d case%d field mismatch\n",mode,i);return 1;}
  entityBefore.s.eFlags=entity.s.eFlags;clientBefore.ps.eFlags=client.ps.eFlags;clientBefore.pers.ready=client.pers.ready;
  memcpy(rawBefore+8,raw+8,4);memcpy(rawClientBefore+0x68,rawClient+0x68,4);memcpy(rawClientBefore+0xafc,rawClient+0xafc,4);
  if(!unchanged()){fprintf(stderr,"ready mode%d case%d unexpected write\n",mode,i);return 1;}n++;
 }
 for(i=0;i<4096;i++){
  setup(i);if(i&1){entity.client=NULL;*(void**)(raw+0x190)=NULL;}snapshot();sound(raw);G_SetClientSound(&entity);
  if(entity.s.loopSound!=*(int*)(raw+0xa0)||entity.s.loopSound){fprintf(stderr,"client sound case%d mismatch\n",i);return 1;}
  entityBefore.s.loopSound=0;memset(rawBefore+0xa0,0,4);if(!unchanged()){fprintf(stderr,"client sound case%d unexpected write\n",i);return 1;}
 }
 setup(0);snapshot();command(NULL);Cmd_IntermissionReady_f(NULL);if(!unchanged())return 1;
 entity.client=NULL;*(void**)(raw+0x190)=NULL;snapshot();command(raw);Cmd_IntermissionReady_f(&entity);if(!unchanged())return 1;
 printf("%d whole ready-chain comparisons (4096 each), 4096 sound resets and 2 NULL guard cases passed\n",n);return 0;
}
