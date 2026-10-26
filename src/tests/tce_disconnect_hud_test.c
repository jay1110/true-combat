#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_draw.c"
#include <windows.h>
#include <stdio.h>
extern qboolean tce_uiCoordinates;
static int n,commandTime,current,events[128][12];
static int number(void){events[n++][0]=1;return current;}
static qboolean command(int num,usercmd_t*out){events[n][0]=2;events[n++][1]=num;memset(out,0,sizeof(*out));out->serverTime=commandTime;return qtrue;}
static const char*translate(const char*t){events[n][0]=3;events[n++][1]=strcmp(t,"Connection Interrupted");return "Connection Interrupted";}
static void color(const float*c){int*p=events[n++];p[0]=4;p[1]=c==NULL;if(c)memcpy(p+2,c,16);}
static void stretch(float x,float y,float w,float h,float s,float t,float s2,float t2,int shader){int*p=events[n++];p[0]=5;memcpy(p+1,&x,4);memcpy(p+2,&y,4);memcpy(p+3,&w,4);memcpy(p+4,&h,4);memcpy(p+5,&s,4);memcpy(p+6,&t,4);memcpy(p+7,&s2,4);memcpy(p+8,&t2,4);p[9]=shader;}static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE m;byte*b,raw[0x600];snapshot_t snap;void(__cdecl*f)(void);int k,count,copy[128][12];if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;b=(byte*)m;f=(void*)(b+0x237e0);
#define H(off,fn,to) hook(b+off,to);hook(fn,to)
 H(0x655f0,trap_GetCurrentCmdNumber,number);H(0x65600,trap_GetUserCmd,command);H(0x2a830,CG_TranslateString,translate);H(0x65420,trap_R_SetColor,color);H(0x65440,trap_R_DrawStretchPic,stretch);
 cg.snap=&snap;*(void**)(b+0x40352c4)=raw;
 for(k=0;k<8192;k++){cg.demoPlayback=*(int*)(b+0x40352b0)=k%2;cg_timescale.value=*(float*)(b+0x4011248)=(k/2)%3==0?1.f:.7f;cg.serverRespawning=*(int*)(b+0x40a2448)=(k/6)%2;cg.time=*(int*)(b+0x407dea4)=k-2048;snap.ps.commandTime=*(int*)(raw+0x2c)=cg.time-3;commandTime=cg.time+(k/12)%7-4;current=k%141-70;cgs.media.disconnectIcon=*(int*)(b+0x259d1dc)=k+37;
 cgs.glconfig.vidWidth=*(int*)(b+0x257e0c8)=k%3==0?960:k%3==1?1280:1920;cgs.glconfig.vidHeight=*(int*)(b+0x257e0cc)=k%3==0?720:k%3==1?720:1080;cgs.screenXScale=*(float*)(b+0x257e0e8)=cgs.glconfig.vidWidth/640.f;cgs.screenYScale=*(float*)(b+0x257e0ec)=cgs.glconfig.vidHeight/480.f;cgs.media.menucharsetShader=*(int*)(b+0x2588008)=117;tce_uiCoordinates=k%2;
 n=0;memset(events,0,sizeof(events));f();count=n;memcpy(copy,events,sizeof(copy));n=0;memset(events,0,sizeof(events));CG_DrawDisconnect();if(tce_uiCoordinates!=k%2||n!=count||memcmp(copy,events,sizeof(copy))){printf("disconnect case%d calls%d/%d\n",k,count,n);{int q;for(q=0;q<sizeof(copy)/4;q++)if(((int*)copy)[q]!=((int*)events)[q]){printf("word%d expected%08x actual%08x\n",q,((int*)copy)[q],((int*)events)[q]);break;}}return 1;}}
 puts("8192 whole disconnect HUD comparisons passed");return 0;}
