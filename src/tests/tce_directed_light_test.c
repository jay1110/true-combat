#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include "../cgame/tce_lightgrid.h"
#include <windows.h>
#include <stdio.h>
void QDECL Com_Error(int code,const char *fmt,...) {exit(2);}
void QDECL Com_Printf(const char *fmt,...) {}
static vec3_t ambient,directed,direction;
static int count;
static void sampleOriginal(const float *point,float *a,float *d,float *n) {
    VectorCopy(ambient,a);VectorCopy(directed,d);VectorCopy(direction,n);++count;
}
static void sampleSource(const tce_lightGrid_t *grid,const float *point,float *a,float *d,float *n) {
    sampleOriginal(point,a,d,n);
}
static void hook(void *at,void *fn) {DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char **argv) {
    HMODULE dll;byte*base;unsigned rng=8732;int i,j,k;vec3_t point={3,5,7},normal;float a[5],b[5];
    void(__cdecl*original)(const float*,const float*,float*);
    if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;original=(void*)(base+0x68200);
    hook(base+0x682e0,sampleOriginal);hook(TCE_CG_LightForPoint,sampleSource);
    for(k=0;k<100000;++k) {
        for(j=0;j<3;++j) {rng=rng*1664525u+1013904223u;ambient[j]=(int)(rng%1000)-200.f;rng=rng*1664525u+1013904223u;directed[j]=(int)(rng%800)-100.f;
            rng=rng*1664525u+1013904223u;direction[j]=((int)(rng%2001)-1000)*.001f;rng=rng*1664525u+1013904223u;normal[j]=((int)(rng%4001)-2000)*.001f;}
        if(k%13==0)VectorClear(normal);
        for(i=0;i<5;++i)a[i]=b[i]=91.5f;
        count=0;original(point,normal,a+1);TCE_CG_LightForParticleDirected(NULL,point,normal,b+1);
        if(count!=2)return 3;
        for(i=0;i<5;++i)if(!_finite(a[i])||!_finite(b[i])||fabs(a[i]-b[i])>.00002+fabs(a[i])*.000002){printf("case%d field%d %.9g %.9g\n",k,i,b[i],a[i]);return 1;}
    }
    printf("100000 directed impact-light original comparisons passed\n");return 0;
}
