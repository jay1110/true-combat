#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_main.c"
#include <windows.h>
#include <stdarg.h>
static char events[32768],players[64][1024],serverInfo[128];
static int clientNumber,selected;
static void patch(void *at,void *fn){DWORD o,t;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void event(const char *fmt,...){va_list ap;va_start(ap,fmt);vsprintf(events+strlen(events),fmt,ap);va_end(ap);}
static void client(uiClientState_t *c){event("client;");memset(c,0,sizeof(*c));c->clientNum=clientNumber;}
static int config(int id,char *buffer,int size){event("config:%d,%d;",id,size);Q_strncpyz(buffer,id==0?serverInfo:id>=689&&id<753?players[id-689]:"INVALID",size);return 1;}
static void set(const char *key,const char *value){event("set:%s=%s;",key,value);if(!strcmp(key,"cg_selectedPlayer"))selected=atoi(value);}
static float value(const char *key){event("get:%s;",key);return (float)selected;}
static int same(unsigned char *b){return uiInfo.playerNumber==*(int *)(b+0x2b34a0)&&uiInfo.teamLeader==*(int *)(b+0x2b34a4)&&uiInfo.playerCount==*(int *)(b+0x2b348c)&&uiInfo.myTeamCount==*(int *)(b+0x2b3490)&&!memcmp(uiInfo.playerNames,b+0x2b34a8,sizeof(uiInfo.playerNames))&&!memcmp(uiInfo.playerMuted,b+0x2b46a8,sizeof(uiInfo.playerMuted))&&!memcmp(uiInfo.playerRefereeStatus,b+0x2b47a8,sizeof(uiInfo.playerRefereeStatus))&&!memcmp(uiInfo.teamNames,b+0x2b48a8,sizeof(uiInfo.teamNames))&&!memcmp(uiInfo.teamClientNums,b+0x2b51a8,sizeof(uiInfo.teamClientNums));}
static void seed(unsigned char *b){memset(uiInfo.playerNames,0x35,sizeof(uiInfo.playerNames));memset(b+0x2b34a8,0x35,sizeof(uiInfo.playerNames));memset(uiInfo.playerMuted,0,sizeof(uiInfo.playerMuted));memset(b+0x2b46a8,0,sizeof(uiInfo.playerMuted));memset(uiInfo.playerRefereeStatus,0,sizeof(uiInfo.playerRefereeStatus));memset(b+0x2b47a8,0,sizeof(uiInfo.playerRefereeStatus));memset(uiInfo.teamNames,0x35,sizeof(uiInfo.teamNames));memset(b+0x2b48a8,0x35,sizeof(uiInfo.teamNames));memset(uiInfo.teamClientNums,0x35,sizeof(uiInfo.teamClientNums));memset(b+0x2b51a8,0x35,sizeof(uiInfo.teamClientNums));}
int main(int argc,char **argv){HMODULE h;unsigned char *b;int n,v,leader,s,k,i,a,c,expectedSelected,cases=0;int keys[]={K_MOUSE1,K_MOUSE2,K_ENTER,K_KP_ENTER,K_LEFTARROW,0};char expected[32768];float special=3.5f;
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
 patch(b+0x20ab0,client);patch(trap_GetClientState,client);patch(b+0x20af0,config);patch(trap_GetConfigString,config);patch(b+0x20530,set);patch(trap_Cvar_Set,set);patch(b+0x20550,value);patch(trap_Cvar_VariableValue,value);
 for(n=0;n<=64;n+=4)for(v=0;v<3;++v)for(leader=0;leader<2;++leader)for(s=-1;s<5;++s)for(k=-1;k<6;++k){
  sprintf(serverInfo,"\\sv_maxclients\\%d",n);clientNumber=n>0?n-1:0;
  for(i=0;i<64;++i){sprintf(players[i],"\\n\\^%dPlayer^^x%d^7\\t\\%d\\tl\\%d\\mu\\%d\\ref\\%d",i%8,i,(i+v)%4,leader,i%3,i%4);if(v==2&&i%3==0)players[i][0]=0;}
  seed(b);selected=s;events[0]=0;
  if(k<0){((void(__cdecl *)(void))(b+0x104d0))();a=0;}else a=((int(__cdecl *)(int,float *,int))(b+0x11500))(7,&special,keys[k]);
  strcpy(expected,events);expectedSelected=selected;selected=s;events[0]=0;
  if(k<0){UI_BuildPlayerList();c=0;}else c=UI_SelectedPlayer_HandleKey(7,&special,keys[k]);
  if(a!=c||expectedSelected!=selected||strcmp(expected,events)||!same(b)){printf("player list mismatch n%d v%d leader%d selected%d key%d\n%s\n%s\n",n,v,leader,s,k,expected,events);return 1;}++cases;
 }
 printf("%d whole original UI player-list/key comparisons passed\n",cases);return 0;
}
