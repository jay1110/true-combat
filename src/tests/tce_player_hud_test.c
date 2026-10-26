#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../cgame/tce_player_hud.h"
typedef struct {int kind, shader; float v[20];} event_t;
static event_t events[32],reference[32];
static int count;
static void check(int ok) {if(!ok){fprintf(stderr,"HUD comparison failed\n");exit(1);}}
static event_t *event(int kind) {event_t *e;check(count<32);e=&events[count++];memset(e,0,sizeof(*e));e->kind=kind;return e;}
static void color(const float *c) {event_t *e=event(c?1:2);if(c)memcpy(e->v,c,16);}
static void pic(float x,float y,float w,float h,int sh) {event_t *e=event(3);e->v[0]=x;e->v[1]=y;e->v[2]=w;e->v[3]=h;e->shader=sh;}
static void bar(float x,float y,float w,float h,float *fg,float *fg2,const float *bg,float fraction,int flags) {event_t *e=event(4);e->v[0]=x;e->v[1]=y;e->v[2]=w;e->v[3]=h;memcpy(e->v+4,fg,16);memcpy(e->v+8,bg,16);e->v[12]=fraction;e->shader=flags;check(fg2==NULL);}
static void compare(int n) {int i,j;check(count==n);for(i=0;i<n;++i){check(events[i].kind==reference[i].kind&&events[i].shader==reference[i].shader);for(j=0;j<20;++j)check(fabs(events[i].v[j]-reference[i].v[j])<.0001);}}
static void hook(unsigned char *p,void *fn) {DWORD old,tmp;check(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old));p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char **argv) {
    HMODULE dll;unsigned char *b;int d,k,n,tests=0; unsigned char snapshot[0x500]={0};
    int damage[3],shaders[4]={701,702,703,704};float r[4]={24,426,20,48};
    void (__cdecl *health)(float *);void (__cdecl *stamina)(float *);
    check(argc==2);dll=LoadLibraryA(argv[1]);check(dll!=NULL);b=(unsigned char *)dll;
    health=(void *)(b+0x29100);stamina=(void *)(b+0x29460);
    hook(b+0x65420,color);hook(b+0x29ea0,pic);hook(b+0x298b0,bar);
    *(void **)(b+0x40352c4)=snapshot;memcpy(b+0x25886bc,shaders,16);
    for(k=0;k<3;++k)for(d=-10;d<=110;++d){
        memset(damage,0,sizeof(damage));damage[k]=d;memcpy(snapshot+0x3c4,damage,12);
        count=0;health(r);n=count;memcpy(reference,events,sizeof(events));count=0;
        TCE_DrawPlayerHealthMan(r,damage,shaders,color,pic);compare(n);++tests;
    }
    r[0]=12;r[1]=423;r[2]=8;r[3]=37;
    *(int *)(b+0x259d184)=711;*(int *)(b+0x25886d0)=712;
    for(d=-1000;d<=21000;d+=31){
        *(int *)(b+0x4087ef8)=d;count=0;stamina(r);n=count;memcpy(reference,events,sizeof(events));count=0;
        TCE_DrawStaminaBar(r,d,711,712,color,pic,bar);compare(n);++tests;
    }
    FreeLibrary(dll);printf("%d original health/stamina HUD comparisons passed.\n",tests);return 0;
}
