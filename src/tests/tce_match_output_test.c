#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern void G_delayPrint(gentity_t*);
static byte raw[0x57c];static gentity_t ent;static byte*base;
static int side,calls,bad,destinations[4],soundIndex,spawnCalls,tempCalls,indexCalls;
static char messages[4][2048];static char *soundName;
static int hook(void*p,void*f){DWORD o,t;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);return 1;}
static void send(int client,const char *s){if(calls>=4){bad=1;return;}destinations[calls]=client;Q_strncpyz(messages[calls++],s,2048);}
static void *spawn(void){spawnCalls++;return side?(void*)&ent:(void*)raw;}
static void *temp(float*origin,int event){tempCalls++;if(event!=(side?EV_GLOBAL_SOUND:53)||memcmp(origin,side?(void*)level.intermission_origin:(void*)(base+0x7ffc6c),12))bad=1;return side?(void*)&ent:(void*)raw;}
static int sound(char *name){indexCalls++;if(tempCalls!=1||name!=soundName)bad=1;return soundIndex;}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*printFn)(char*,void*);void(__cdecl*soundFn)(char*);void(__cdecl*spawnFn)(int,int,void*);int i,j,type;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;printFn=(void*)(base+0x65120);soundFn=(void*)(base+0x651b0);spawnFn=(void*)(base+0x65400);
 if(!hook(base+0x8ad60,send)||!hook(trap_SendServerCommand,send)||!hook(base+0x94e30,spawn)||!hook(G_Spawn,spawn)||!hook(base+0x94fa0,temp)||!hook(G_TempEntity,temp)||!hook(base+0x947f0,sound)||!hook(G_SoundIndex,sound))return 2;
 for(i=0;i<4096;i++){
  char text[1024],expected[4][2048];int clients[4],count,ix=(i%65)-1;
  for(j=0;j<i%900;j++)text[j]="Ab09 ^1\"\\\n%"[(j+i)%11];text[j]=0;
  calls=bad=0;printFn(text,ix<0?NULL:base+0x226e2a0+ix*0x57c);count=calls;memcpy(clients,destinations,sizeof(clients));memcpy(expected,messages,sizeof(expected));if(bad)return 3;
  calls=bad=0;G_printFull(text,ix<0?NULL:g_entities+ix);
  if(bad||calls!=2||count!=calls||memcmp(clients,destinations,2*sizeof(int))||strcmp(expected[0],messages[0])||strcmp(expected[1],messages[1])){fprintf(stderr,"full print case%d failed\n",i);return 1;}
 }
 for(i=0;i<4096;i++){
  char name[64];sprintf(name,"sound/test/%d.wav",i);soundName=name;soundIndex=i-2;
  memset(raw,0,sizeof(raw));memset(&ent,0,sizeof(ent));ent.r.svFlags=*(int*)(raw+0x128)=i*17;
  for(j=0;j<3;j++)level.intermission_origin[j]=((float*)(base+0x7ffc6c))[j]=(float)(i+j)/7;
  side=0;bad=tempCalls=indexCalls=0;soundFn(name);if(bad||tempCalls!=1||indexCalls!=1)return 3;
  side=1;bad=tempCalls=indexCalls=0;G_globalSound(name);
  if(bad||tempCalls!=1||indexCalls!=1||ent.s.eventParm!=*(int*)(raw+0xbc)||ent.r.svFlags!=*(int*)(raw+0x128)){fprintf(stderr,"global sound case%d failed\n",i);return 1;}
 }
 for(type=0;type<9;type++)for(i=0;i<1024;i++){
  void*owner=i&1?(void*)(base+0x226e2a0+(i%64)*0x57c):NULL;gentity_t*nowner=i&1?g_entities+i%64:NULL;
  memset(raw,0,sizeof(raw));memset(&ent,0,sizeof(ent));ent.r.svFlags=*(int*)(raw+0x128)=i*7;ent.s.eFlags=*(int*)(raw+8)=i*13;ent.clipmask=*(int*)(raw+0x1d4)=-1;
  level.time=*(int*)(base+0x7febe0)=i*111;side=0;spawnCalls=0;spawnFn(type,i*99,owner);if(spawnCalls!=1)return 3;
  side=1;spawnCalls=0;G_spawnPrintf(type,i*99,nowner);
  if(spawnCalls!=1||strcmp(ent.classname,*(char**)(raw+0x1a4))||ent.clipmask!=*(int*)(raw+0x1d4)||ent.s.eType!=*(int*)(raw+4)||ent.r.svFlags!=*(int*)(raw+0x128)||ent.s.eFlags!=*(int*)(raw+8)||ent.spawnflags!=*(int*)(raw+0x1a8)||ent.timestamp!=*(int*)(raw+0x24c)||ent.nextthink!=*(int*)(raw+0x29c)||ent.parent!=nowner||*(void**)(raw+0x218)!=owner||ent.think!=G_delayPrint||*(void**)(raw+0x2a4)!=base+0x651f0){fprintf(stderr,"spawn print type%d case%d failed\n",type,i);return 1;}
 }
 puts("4096 whole full-print, 4096 global-sound and 9216 delayed-print constructor comparisons passed");return 0;
}

