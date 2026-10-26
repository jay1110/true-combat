#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_main.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
static char trace[256];static float killed,campaignmap;
static float value(const char*n){strcat(trace,n);strcat(trace,";");if(!strcmp(n,"sv_killserver"))return killed;assert(!strcmp(n,"ui_campaignmap"));return campaignmap;}
static void sound(int s,int c){sprintf(trace+strlen(trace),"S%d,%d;",s,c);}
static struct{unsigned char*p,bytes[5];}hooks[4];static int hc;
static void patch(void*a,void*f){unsigned char*p=a;DWORD o,t;assert(hc<4);hooks[hc].p=p;memcpy(hooks[hc++].bytes,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].bytes,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}


int main(int argc,char**argv){HMODULE h;unsigned char*b;int k,j,flags,ret,played,cases=0;unsigned seed=12345;char expected[256];int bits[]={1,2,4,8,16,32,64,128,256,512,1024,2048,4096,8192,16384,32768,65536,131072,262144,524288,1048576,0x1000000,0x2000000,0x4000000,0x8000000,0x20000000};
assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;patch(b+0x20550,value);patch(trap_Cvar_VariableValue,value);patch(b+0x20930,sound);patch(trap_S_StartLocalSound,sound);
for(k=0;k<65536;++k){seed=seed*1664525u+1013904223u;flags=k<26?bits[k]:(int)(seed&0x2f1fffff);uiInfo.teamLeader=*(int*)(b+0x2b34a4)=k%2;uiInfo.myTeamCount=*(int*)(b+0x2b3490)=2;ui_selectedPlayer.integer=*(int*)(b+0x2dd3ac)=k%3;uiInfo.playerNumber=*(int*)(b+0x2b34a0)=k%3;for(j=0;j<3;++j)uiInfo.teamClientNums[j]=((int*)(b+0x2b51a8))[j]=j;
ui_netSource.integer=*(int*)(b+0x28fd4c)=(k/2)%3;uiInfo.uiDC.realTime=*(int*)(b+0x292140)=100;uiInfo.newHighScoreTime=*(int*)(b+0x2b2058)=99+k%3;uiInfo.newBestTime=*(int*)(b+0x2b205c)=99+(k/3)%3;uiInfo.soundHighScore=*(int*)(b+0x2b206c)=(k/3)%2;uiInfo.newHighScoreSound=*(int*)(b+0x2d3d50)=27;uiInfo.demoAvailable=*(int*)(b+0x2b2068)=(k/5)%2;
ui_currentCampaign.integer=*(int*)(b+0x2d902c)=0;uiInfo.campaignList[0].mapCount=*(int*)(b+0x2b96c4)=k%8;uiInfo.campaignList[0].progress=*(int*)(b+0x2b9714)=(k/7)%4;uiInfo.playerIndex=*(int*)(b+0x2b349c)=0;uiInfo.playerMuted[0]=*(int*)(b+0x2b46a8)=(k/11)%2;uiInfo.playerRefereeStatus[0]=*(int*)(b+0x2b47a8)=(k/13)%3;killed=(float)((k/17)%2);campaignmap=(float)((k/19)%5)+.75f;
trace[0]=0;ret=((int(__cdecl*)(int))(b+0x6270))(flags);played=*(int*)(b+0x2b206c);strcpy(expected,trace);trace[0]=0;if(UI_OwnerDrawVisible(flags)!=ret||uiInfo.soundHighScore!=played||strcmp(expected,trace)){fprintf(stderr,"visibility mismatch case%d mask%x\nO%s\nS%s\n",k,flags,expected,trace);return 1;}++cases;}
restore();assert(FreeLibrary(h));printf("%d original owner-visibility comparisons passed\n",cases);return 0;}
