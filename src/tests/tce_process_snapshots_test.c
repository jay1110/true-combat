#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_snapshot.c"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
static byte*base;static int originalRun,number,queueCount,readIndex,fault,count;static int events[64][2];static char errorText[128];static jmp_buf escape;
static byte raw[8][0x600];static snapshot_t native[8];
static void*ptr(int i){return i<0?NULL:originalRun?(void*)raw[i]:(void*)&native[i];}
static int indexOf(void*p){int i;if(!p)return -1;for(i=0;i<8;i++)if(p==ptr(i))return i;return -2;}
static void record(int op,int n){events[count][0]=op;events[count++][1]=n;}
static void current(int*n,int*t){record(1,0);*n=number;*t=9876;}
static void*readNext(void){int i=readIndex++;record(2,i);return i<queueCount?ptr(i):NULL;}
static void initial(void*p){record(3,indexOf(p));if(originalRun)*(void**)(base+0x40352c4)=p;else cg.snap=p;}
static void setNext(void*p){record(4,indexOf(p));if(originalRun)*(void**)(base+0x40352c8)=p;else cg.nextSnap=p;}
static void transition(void){void*p=originalRun?*(void**)(base+0x40352c8):cg.nextSnap;record(5,indexOf(p));if(originalRun){*(void**)(base+0x40352c4)=fault==1?NULL:p;*(void**)(base+0x40352c8)=NULL;}else{cg.snap=fault==1?NULL:p;cg.nextSnap=NULL;}}
static void error(const char*s,...){Q_strncpyz(errorText,s,sizeof(errorText));longjmp(escape,1);}
static void hook(void*at,void*fn){DWORD old,t;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t);}
#define H(o,n,f) hook(base+o,f);hook(n,f)
int main(int argc,char**argv){HMODULE m;void(__cdecl*original)(void);int k,i,pass,n,expected[6],ecopy[64][2];char expectedError[128];if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;base=(byte*)m;original=(void*)(base+0x5ff70);
 H(0x65590,trap_GetCurrentSnapshotNumber,current);H(0x605e0,CG_ReadNextSnapshot,readNext);H(0x5fca0,CG_SetInitialSnapshot,initial);H(0x60490,CG_SetNextSnap,setNext);H(0x60090,CG_TransitionSnapshot,transition);H(0x47bf0,CG_Error,error);
 for(k=0;k<4096;k++){
  queueCount=k%5;number=(k/5)%4;fault=(k%31==0 && queueCount==0);memset(raw,0,sizeof(raw));memset(native,0,sizeof(native));
  for(i=0;i<8;i++){*(int*)(raw[i]+8)=native[i].serverTime=i*100-100;*(int*)raw[i]=native[i].snapFlags=((k>>i)&1)?2:0;}
  if(k%17==0)*(int*)(raw[1]+8)=native[1].serverTime=-200;
  for(pass=0;pass<2;pass++){
   originalRun=!pass;readIndex=0;count=0;errorText[0]=0;
   cg.latestSnapshotNum=*(int*)(base+0x40352bc)=(k/20)%3;cg.latestSnapshotTime=*(int*)(base+0x40352c0)=7;
   cg.time=*(int*)(base+0x407dea4)=(k/60)%7*100-150;cgDC.realTime=*(int*)(base+0x4014cc0)=39;
   cg.snap=(k&128)?&native[6]:NULL;*(void**)(base+0x40352c4)=(k&128)?raw[6]:NULL;
   cg.nextSnap=(k&256)?&native[7]:NULL;*(void**)(base+0x40352c8)=(k&256)?raw[7]:NULL;
   if(!setjmp(escape)){if(originalRun)original();else CG_ProcessSnapshots();}
   if(!pass){n=count;memcpy(ecopy,events,sizeof(ecopy));strcpy(expectedError,errorText);expected[0]=*(int*)(base+0x40352bc);expected[1]=*(int*)(base+0x40352c0);expected[2]=*(int*)(base+0x407dea4);expected[3]=*(int*)(base+0x4014cc0);expected[4]=indexOf(*(void**)(base+0x40352c4));expected[5]=indexOf(*(void**)(base+0x40352c8));}
   else if(n!=count||memcmp(ecopy,events,n*sizeof(events[0]))||strcmp(expectedError,errorText)||expected[0]!=cg.latestSnapshotNum||expected[1]!=cg.latestSnapshotTime||expected[2]!=cg.time||expected[3]!=cgDC.realTime||expected[4]!=indexOf(cg.snap)||expected[5]!=indexOf(cg.nextSnap)){printf("process snapshots case%d\n",k);return 1;}
  }
 }puts("4096 whole CG_ProcessSnapshots comparisons passed");return 0;}
