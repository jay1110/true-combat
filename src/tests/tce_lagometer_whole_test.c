#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_draw.c"
#include <windows.h>
#include <stdio.h>
extern qboolean tce_uiCoordinates;
static int n,events[1024][12];
static void color(const float*c){int*p=events[n++];p[0]=1;p[1]=!c;if(c)memcpy(p+2,c,16);}
static void stretch(float x,float y,float w,float h,float s,float t,float s2,float t2,int shader){int*p=events[n++];p[0]=2;memcpy(p+1,&x,4);memcpy(p+2,&y,4);memcpy(p+3,&w,4);memcpy(p+4,&h,4);memcpy(p+5,&s,4);memcpy(p+6,&t,4);memcpy(p+7,&s2,4);memcpy(p+8,&t2,4);p[9]=shader;}
static void big(int x,int y,const char*t,float a){int*p=events[n++];p[0]=3;p[1]=x;p[2]=y;p[3]=strcmp(t,"snc");memcpy(p+4,&a,4);}
static void disconnect(void){events[n++][0]=4;}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE m;byte*b;void(__cdecl*frame)(void),(__cdecl*draw)(void);void(__cdecl*snapshot)(snapshot_t*);snapshot_t snap;lagometer_t expected;int k,i,count,copy[1024][12];
 if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;b=(byte*)m;frame=(void*)(b+0x21080);snapshot=(void*)(b+0x210b0);draw=(void*)(b+0x23470);
#define H(off,fn,to) hook(b+off,to);hook(fn,to)
 H(0x65420,trap_R_SetColor,color);H(0x65440,trap_R_DrawStretchPic,stretch);H(0x2a4f0,CG_DrawBigString,big);H(0x237e0,CG_DrawDisconnect,disconnect);
 for(k=0;k<4096;k++){
  memset(&lagometer,0x5a,sizeof(lagometer));lagometer.frameCount=k-2000;lagometer.snapshotCount=k-1000;memcpy(b+0x4b90f60,&lagometer,sizeof(lagometer));cg.time=*(int*)(b+0x407dea4)=k;cg.latestSnapshotTime=*(int*)(b+0x40352c0)=k+(k%131)-65;snap.ping=k%1101-1;snap.snapFlags=k%4;
  frame();CG_AddLagometerFrameInfo();if(memcmp(b+0x4b90f60,&lagometer,sizeof(lagometer))){printf("frame case%d\n",k);return 1;}
  snapshot(k%3?&snap:NULL);CG_AddLagometerSnapshotInfo(k%3?&snap:NULL);if(memcmp(b+0x4b90f60,&lagometer,sizeof(lagometer))){printf("snapshot case%d\n",k);return 1;}
 }
 for(k=0;k<2048;k++){
  for(i=0;i<128;i++){lagometer.frameSamples[i]=(k+i*19)%901-450;lagometer.snapshotSamples[i]=(k+i*37)%1203-2;lagometer.snapshotFlags[i]=(k+i)%4;}lagometer.frameCount=k-1000;lagometer.snapshotCount=k+37;expected=lagometer;memcpy(b+0x4b90f60,&lagometer,sizeof(lagometer));
  cg_lagometer.integer=*(int*)(b+0x4b32a4c)=k%3;cgs.localServer=*(int*)(b+0x257e0fc)=k%5==0;cg_nopredict.integer=*(int*)(b+0x488d02c)=k%2;cgs.glconfig.vidWidth=*(int*)(b+0x257e0c8)=k%2?1280:960;cgs.glconfig.vidHeight=*(int*)(b+0x257e0cc)=720;cgs.screenXScale=*(float*)(b+0x257e0e8)=cgs.glconfig.vidWidth/640.f;cgs.screenYScale=*(float*)(b+0x257e0ec)=1.5f;cgs.media.lagometerShader=*(int*)(b+0x2588140)=113;cgs.media.whiteShader=*(int*)(b+0x2588018)=114;tce_uiCoordinates=k%2;
  n=0;memset(events,0,sizeof(events));draw();count=n;memcpy(copy,events,sizeof(copy));n=0;memset(events,0,sizeof(events));CG_DrawLagometer();if(n!=count||memcmp(copy,events,sizeof(copy))||memcmp(&expected,&lagometer,sizeof(expected))||memcmp(&expected,b+0x4b90f60,sizeof(expected))||tce_uiCoordinates!=k%2){printf("lag draw case%d calls%d/%d\n",k,count,n);for(i=0;i<sizeof(copy)/4;i++)if(((int*)copy)[i]!=((int*)events)[i]){printf("word%d expected%08x actual%08x\n",i,((int*)copy)[i],((int*)events)[i]);break;}return 1;}
 }puts("10240 whole lagometer comparisons passed (4096 frame,4096 snapshot,2048 renderer)");return 0;}
