#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
typedef struct{int id,v[6];char text[128];} event_t;static event_t events[16];static int count,loaded;
static event_t*add(int id){event_t*e=&events[count++];memset(e,0,sizeof(*e));e->id=id;return e;}
static int load(int id,const char*n){event_t*e=add(1);e->v[0]=id;Q_strncpyz(e->text,n,sizeof(e->text));return loaded;}
static void start(int id,int time){event_t*e=add(2);e->v[0]=id;e->v[1]=time;}
static void stop(int id){add(3)->v[0]=id;}
static void send(const char*n){Q_strncpyz(add(4)->text,n,128);}
static void cvar(const char*n,const char*v){event_t*e=add(5);Q_strncpyz(e->text,n,128);e->v[0]=atoi(v);}
static void fade(int r,int g,int b,int a,int time,int duration){event_t*e=add(6);e->v[0]=r;e->v[1]=g;e->v[2]=b;e->v[3]=a;e->v[4]=time;e->v[5]=duration;}
static void print(const char*fmt,...){Q_strncpyz(add(7)->text,fmt,128);}
static void hook(void*at,void*fn){DWORD old,t;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t);}
#define H(o,n,f) hook(b+o,f);hook(n,f)
int main(int argc,char**argv){HMODULE m;byte*b;void(__cdecl*originalStart)(const char*,int);void(__cdecl*originalStop)(void);static const char*names[]={"","intro","intro.camera","northport.scene.camera","dir/name","a.b/c.d","UPPER.CAMERA"};int k,op,n,state;event_t expected[16];if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;b=(byte*)m;originalStart=(void*)(b+0x1c140);originalStop=(void*)(b+0x1c250);
 H(0x65670,trap_loadCamera,load);H(0x65690,trap_startCamera,start);H(0x656b0,trap_stopCamera,stop);H(0x64da0,trap_SendClientCommand,send);H(0x64c10,trap_Cvar_Set,cvar);H(0x21560,CG_Fade,fade);H(0x47b10,CG_Printf,print);
 for(k=0;k<1792;k++)for(op=0;op<2;op++){loaded=(k/7)%2;cg.time=*(int*)(b+0x407dea4)=k*31-17000;cg.cameraMode=*(int*)(b+0x408110c)=k%3;count=0;if(op)originalStop();else originalStart(names[k%7],(k/14)%3-1);n=count;memcpy(expected,events,sizeof(expected));state=*(int*)(b+0x408110c);count=0;if(op)CG_StopCamera();else CG_StartCamera(names[k%7],(k/14)%3-1);if(state!=cg.cameraMode||n!=count||memcmp(expected,events,n*sizeof(event_t))){printf("camera command case%d op%d\n",k,op);return 1;}}
 puts("3584 whole StartCamera/StopCamera comparisons passed (1792 each)");return 0;}
