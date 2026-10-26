#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
static byte *base,raw[0x57c],clients[64*0x16b0];
static gentity_t ent;static gclient_t nativeClients[64];
static int side,count,variant,mode,bad;
typedef struct {int index;char text[1024];} call;
typedef struct {int count;call calls[64];} result;
static result results[2];
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static void *spawn(void){count++;if(side)level.time+=7;else *(int*)(base+0x7febe0)+=7;return side?(void*)&ent:raw;}
static void linkEntity(void*p){int i;byte expected[0x57c];gentity_t n;count++;if(side){n=ent;n.s.eType=0;n.classname=NULL;n.eventTime=0;n.r.eventTime=0;n.freeAfterEvent=0;n.r.svFlags=0;n.s.effect1Time=0;for(i=0;i<sizeof(n);i++)if(((byte*)&n)[i])bad=1;if(p!=&ent||ent.s.eType!=ET_EVENTS+EV_POPUPMESSAGE||strcmp(ent.classname,"messageent")||ent.eventTime!=variant+7||ent.r.eventTime!=variant+7||ent.freeAfterEvent!=1||ent.r.svFlags!=32||ent.s.effect1Time!=mode)bad=1;ent.s.density=99;}else{memcpy(expected,raw,sizeof(raw));memset(expected+4,0,4);memset(expected+0x1a4,0,4);memset(expected+0x1c0,0,8);memset(expected+0x184,0,4);memset(expected+0x128,0,4);memset(expected+0x10c,0,4);for(i=0;i<sizeof(expected);i++)if(expected[i])bad=1;if(p!=raw||*(int*)(raw+4)!=193||strcmp(*(char**)(raw+0x1a4),"messageent")||*(int*)(raw+0x1c0)!=variant+7||*(int*)(raw+0x184)!=variant+7||*(int*)(raw+0x1c4)!=1||*(int*)(raw+0x128)!=32||*(int*)(raw+0x10c)!=mode)bad=1;*(int*)(raw+0xf4)=99;}}
static void sendCommand(int index,const char*text){call*c;if(count>=64){bad=1;return;}c=&results[side].calls[count++];c->index=index;Q_strncpyz(c->text,text,sizeof(c->text));if(mode==1&&index<63){if(side)nativeClients[index+1].sess.sessionTeam=(team_t)(variant%4);else *(int*)(clients+(index+1)*0x16b0+0xb30)=variant%4;}if(mode==2){if(side)level.maxclients=index+1;else *(int*)(base+0x7febd8)=index+1;}}
int main(int argc,char**argv){HMODULE dll;void*(__cdecl*popup)(int);void(__cdecl*team)(int,char*);int v,i,total=0;char text[1024];void*r;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;popup=(void*)(base+0x95050);team=(void*)(base+0x94870);
 if(!hook(base+0x94e30,spawn)||!hook(G_Spawn,spawn)||!hook(base+0x8afa0,linkEntity)||!hook(trap_LinkEntity,linkEntity)||!hook(base+0x8ad60,sendCommand)||!hook(trap_SendServerCommand,sendCommand))return 2;
 for(v=0;v<4096;v++)for(side=0;side<2;side++){variant=(v-2048)*123;mode=(v%32)-8;count=bad=0;memset(raw,0,sizeof(raw));memset(&ent,0,sizeof(ent));level.time=*(int*)(base+0x7febe0)=variant;r=side?(void*)G_PopupMessage((popupMessageType_t)mode):popup(mode);if(bad||count!=2||r!=(side?(void*)&ent:raw)||(side?ent.s.density:*(int*)(raw+0xf4))!=99){printf("Popup mismatch case%d side%d count%d bad%d\n",v,side,count,bad);return 1;}if(side)total++;}
 for(v=0;v<32768;v++)for(side=0;side<2;side++){variant=v;mode=(v/256)%3;count=bad=0;memset(&results[side],0,sizeof(result));memset(clients,0,sizeof(clients));memset(nativeClients,0,sizeof(nativeClients));level.clients=nativeClients;*(void**)(base+0x7feb80)=clients;level.maxclients=*(int*)(base+0x7febd8)=(v/4)%65;for(i=0;i<64;i++){nativeClients[i].pers.connected=(clientConnected_t)((v/260+i)%3);*(int*)(clients+i*0x16b0+0x5ac)=nativeClients[i].pers.connected;nativeClients[i].sess.sessionTeam=(team_t)((v/780+i)%4);*(int*)(clients+i*0x16b0+0xb30)=nativeClients[i].sess.sessionTeam;}for(i=0;i<1023;i++)text[i]="objective %s;\\\"\n"[(i+v)%16];text[1023]=0;if(v&1)text[v%1023]=0;if(side)G_TeamCommand((team_t)(v%4),text);else team(v%4,text);results[side].count=count;if(bad)return 1;if(side){if(memcmp(results,results+1,sizeof(result))){printf("Team command mismatch case%d count%d/%d\n",v,results[0].count,results[1].count);return 1;}total++;}}
 printf("PASS objective notification producers %d cases: 4096 popup, 32768 team broadcast; explicit popup event transport mapping 193 -> ET_EVENTS+EV_POPUPMESSAGE\n",total);return 0;
}
