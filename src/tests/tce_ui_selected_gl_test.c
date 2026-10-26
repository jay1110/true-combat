#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
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
static void textvar(const char*n,char*out,int size){event("str:%s;",n);Q_strncpyz(out,!strcmp(n,"name")?"Local":"Selected",size);}
static void paint(float x,float y,float scale,vec4_t color,const char*t,float adjust,int limit,int style){event("paint:%.9g,%.9g,%.9g,%.9g,%s,%.9g,%d,%d;",x,y,scale,color[0],t,adjust,limit,style);}
int main(int argc,char**argv){HMODULE h;unsigned char*b;int n,leader,s,gate,i,cases=0,es;char expected[32768];rectDef_t r={3,7,80,20};vec4_t color={.2f,.3f,.4f,.5f};
if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char*)h;
patch(b+0x20ab0,client);patch(trap_GetClientState,client);patch(b+0x20af0,config);patch(trap_GetConfigString,config);patch(b+0x20530,set);patch(trap_Cvar_Set,set);patch(b+0x20550,value);patch(trap_Cvar_VariableValue,value);patch(b+0x20570,textvar);patch(trap_Cvar_VariableStringBuffer,textvar);patch(b+0x48f0,paint);patch(Text_Paint,paint);
for(n=0;n<=8;++n)for(leader=0;leader<2;++leader)for(s=-1;s<5;++s)for(gate=-1;gate<=1;++gate){sprintf(serverInfo,"\\sv_maxclients\\%d",n);clientNumber=0;for(i=0;i<64;++i)sprintf(players[i],"\\n\\^1Player%d\\t\\%d\\tl\\%d\\mu\\0\\ref\\0",i,i%2,leader);seed(b);*(int*)(b+0x2b34a4)=uiInfo.teamLeader=leader;*(int*)(b+0x2b3498)=uiInfo.playerRefresh=1000;*(int*)(b+0x292140)=uiInfo.uiDC.realTime=1000+gate;selected=s;events[0]=0;((void(__cdecl*)(rectDef_t*,float,vec4_t,int))(b+0x10460))(&r,.3f,color,2);strcpy(expected,events);es=selected;selected=s;events[0]=0;UI_DrawSelectedPlayer(&r,.3f,color,2);if(strcmp(expected,events)||es!=selected||*(int*)(b+0x2b3498)!=uiInfo.playerRefresh||(gate>0&&!same(b))){printf("selected draw mismatch n%d leader%d selection%d gate%d\n%s\n%s\n",n,leader,s,gate,expected,events);return 1;}++cases;}
for(n=0;n<=132;++n){char input[4096];int count;input[0]=0;for(i=0;i<n;++i)strcat(input,i%3?" EXT_x":"  X\tY ");memset(uiInfo.glInfoLines,0,sizeof(uiInfo.glInfoLines));memset(b+0x2d816c,0,512);strcpy(uiInfo.uiDC.glconfig.extensions_string,input);strcpy((char*)b+0x2b0fe4,input);((void(__cdecl*)(void))(b+0xe4a0))();UI_ParseGLConfig();count=*(int*)(b+0x2d836c);if(count!=uiInfo.numGlInfoLines||memcmp(b+0x2b0fe4,uiInfo.uiDC.glconfig.extensions_string,strlen(input)+1))return 3;for(i=0;i<count-4;++i)if(*(char**)(b+0x2d816c+i*4)-((char*)b+0x2b0fe4)!=uiInfo.glInfoLines[i]-uiInfo.uiDC.glconfig.extensions_string)return 4;++cases;}
printf("%d original selected-player/GL parser comparisons passed\n",cases);return 0;}
