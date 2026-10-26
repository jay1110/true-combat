#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../cgame/tce_levelshot_fade.h"
typedef struct {int kind, shader; float v[8];} event_t;
static event_t events[8],reference[8];
static int count;
static void check(int ok) {if(!ok){fprintf(stderr,"Levelshot fade comparison failed\n");exit(1);}}
static event_t *event(int kind) {event_t *e;check(count<8);e=&events[count++];memset(e,0,sizeof(*e));e->kind=kind;return e;}
static void color(const float *c) {event_t *e=event(c?1:2);if(c)memcpy(e->v,c,16);}
static void pic(float x,float y,float w,float h,int sh) {event_t *e=event(3);e->v[0]=x;e->v[1]=y;e->v[2]=w;e->v[3]=h;e->shader=sh;}
static void fill(float x,float y,float w,float h,const float *c) {event_t *e=event(4);e->v[0]=x;e->v[1]=y;e->v[2]=w;e->v[3]=h;memcpy(e->v+4,c,16);}
static void hook(unsigned char *p,void *fn) {DWORD old,tmp;check(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old));p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char **argv) {
    HMODULE dll;unsigned char *b;int snap,dt,i,j,n,tests=0;
    void (__cdecl *original)(void);
    check(argc==2);dll=LoadLibraryA(argv[1]);check(dll!=NULL);b=(unsigned char *)dll;
    original=(void *)(b+0x276b0);hook(b+0x65420,color);hook(b+0x29ea0,pic);hook(b+0x29850,fill);
    for(snap=0;snap<2;++snap)for(dt=-100;dt<=2100;++dt){
        *(int *)(b+0x40352c4)=snap;*(int *)(b+0x407dea4)=10000+dt;
        *(int *)(b+0x40a4bac)=10000;*(int *)(b+0x25884c0)=765;
        count=0;original();n=count;memcpy(reference,events,sizeof(events));
        count=0;TCE_DrawLevelshotFade(snap,10000+dt,10000,765,color,pic,fill);
        check(count==n);for(i=0;i<n;++i){check(events[i].kind==reference[i].kind&&events[i].shader==reference[i].shader);for(j=0;j<8;++j)check(fabs(events[i].v[j]-reference[i].v[j])<.000001);}
        ++tests;
    }
    FreeLibrary(dll);printf("%d original levelshot fade comparisons passed.\n",tests);return 0;
}
