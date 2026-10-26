#include "../cgame/cg_local.h"
#include "../cgame/tce_smoke_grenade.h"
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
static int cases,count,queries,solidMode;
typedef struct {int kind;vec3_t point;refEntity_t ent;} call_t;
static call_t calls[64],expected[64];
void QDECL Com_Error(int code,const char *fmt,...){exit(2);}
void QDECL Com_Printf(const char *fmt,...){}
static void require(int ok,const char *why){if(!ok){fprintf(stderr,"Smoke case %d: %s\n",cases,why);exit(1);}}
static int closeFloat(float a,float b){return fabs((double)a-b)<=0.00015+fabs((double)b)*0.000004;}
int trap_CM_PointContents(const vec3_t point,clipHandle_t model){call_t *c=&calls[count++];require(count<64&&model==0,"contents contract");memset(c,0,sizeof(*c));c->kind=1;VectorCopy(point,c->point);return solidMode==1?CONTENTS_SOLID:solidMode==2&&queries++%2?CONTENTS_SOLID:CONTENTS_WATER;}
void trap_R_AddRefEntityToScene(const refEntity_t *ent){call_t *c=&calls[count++];require(count<64,"render capacity");memset(c,0,sizeof(*c));c->kind=2;c->ent=*ent;}
typedef struct {unsigned char *address,saved[5];} hook_t;
static void hook(hook_t *h,unsigned char *base,int offset,void *replacement) {
    DWORD protection,unused;h->address=base+offset;memcpy(h->saved,h->address,5);
    require(VirtualProtect(h->address,5,PAGE_EXECUTE_READWRITE,&protection),"hook protection");
    h->address[0]=0xe9;*(int *)(h->address+1)=(unsigned char *)replacement-h->address-5;
    FlushInstructionCache(GetCurrentProcess(),h->address,5);
    require(VirtualProtect(h->address,5,protection,&unused),"hook restore");
}
static void unhook(hook_t *h) {
    DWORD protection,unused;
    require(VirtualProtect(h->address,5,PAGE_EXECUTE_READWRITE,&protection),"unhook protection");
    memcpy(h->address,h->saved,5);FlushInstructionCache(GetCurrentProcess(),h->address,5);
    require(VirtualProtect(h->address,5,protection,&unused),"unhook restore");
}


static void compare(unsigned char *base,tce_lightGrid_t *grid,int start,int elapsed,int large,int solid,int view,int tint,int outside){
    unsigned char raw[0xa94],saved[0xa94];vec3_t origin={17,-23,45},forward={1,0,0},color,initial;
    int i,k,n;const int shaders[]={81,82,83};
    void (__cdecl *original)(void *)=(void *)(base+0x310a0);
    if(outside)origin[0]=19000;
    if(view==1)VectorSet(forward,.2f,-.7f,.4f);if(view==2)VectorSet(forward,0,0,0);
    VectorSet(color,tint*.25f,tint*.125f,tint*.375f);VectorCopy(color,initial);
    memset(raw,0x5a,sizeof(raw));*(int *)(raw+0x54)=start;memcpy(raw+0x710,origin,12);memcpy(raw+0x918,color,12);memcpy(saved,raw,sizeof(raw));
    *(int *)(base+0x407dea4)=start+elapsed;*(int *)(base+0x3fdf380)=large;memcpy(base+0x407f028,forward,12);memcpy(base+0x25885b0,shaders,12);
    solidMode=solid;count=queries=0;original(raw);n=count;memcpy(expected,calls,sizeof(expected));
    count=queries=0;TCE_CG_SmokeGrenadeExtended(grid,start+elapsed,start,large,origin,forward,color,shaders);
    require(count==n,"call count");
    for(i=0;i<n;++i){
        require(calls[i].kind==expected[i].kind,"call order");
        for(k=0;k<3;++k)require(closeFloat(calls[i].point[k],expected[i].point[k]),"contents position");
        for(k=0;k<3;++k){require(closeFloat(calls[i].ent.origin[k],expected[i].ent.origin[k]),"sprite origin");calls[i].ent.origin[k]=expected[i].ent.origin[k];}
        if(!closeFloat(calls[i].ent.radius,expected[i].ent.radius)||!closeFloat(calls[i].ent.rotation,expected[i].ent.rotation)){
            fprintf(stderr,"start=%d elapsed=%d call=%d radius %.9g/%.9g rotation %.9g/%.9g\n",start,elapsed,i,calls[i].ent.radius,expected[i].ent.radius,calls[i].ent.rotation,expected[i].ent.rotation);require(0,"sprite float");}
        calls[i].ent.radius=expected[i].ent.radius;calls[i].ent.rotation=expected[i].ent.rotation;
        if(memcmp(&calls[i].ent,&expected[i].ent,sizeof(refEntity_t))){fprintf(stderr,"start=%d age=%d call=%d RGBA=%d,%d,%d,%d / %d,%d,%d,%d\n",start,elapsed,i,calls[i].ent.shaderRGBA[0],calls[i].ent.shaderRGBA[1],calls[i].ent.shaderRGBA[2],calls[i].ent.shaderRGBA[3],expected[i].ent.shaderRGBA[0],expected[i].ent.shaderRGBA[1],expected[i].ent.shaderRGBA[2],expected[i].ent.shaderRGBA[3]);require(0,"render bytes");}
    }
    for(k=0;k<3;++k)require(closeFloat(color[k],*(float *)(raw+0x918+k*4)),"color cache");
    memcpy(saved+0x918,raw+0x918,12);require(!memcmp(raw,saved,sizeof(raw)),"original extra writes");++cases;
}
int main(int argc,char **argv){
    const int starts[]={0,1,127,754,10000,16777217};
    const int ages[]={0,1,2,99,100,499,500,501,1000,4999,5000,24999,25000,30000,32999,33000,34999,35000,35001,35499,35500,40000};
    tce_lightGrid_t grid;byte samples[512];float sine[1024];HMODULE dll;unsigned char *base;hook_t hooks[2];int a,b,c,d,e,f,g,i;
    require(argc==2,"DLL argument");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"original load");base=(unsigned char *)dll;
    VectorSet(grid.origin,-4096,-4096,-4096);VectorSet(grid.spacing,4096,4096,4096);VectorSet(grid.bounds,4,4,4);grid.samples=samples;grid.sine=sine;
    for(i=0;i<512;++i)samples[i]=(byte)(i*17+5);for(i=0;i<1024;++i)sine[i]=(float)sin(i*6.283185307179586/1024);
    memcpy(base+0x40a4c24,grid.spacing,12);memcpy(base+0x40a4c30,grid.bounds,12);memcpy(base+0x40a4c58,grid.origin,12);memcpy(base+0x40a4c64,samples,512);memcpy(base+0x4845e64,sine,sizeof(sine));
    hook(&hooks[0],base,0x64e20,trap_CM_PointContents);hook(&hooks[1],base,0x65290,trap_R_AddRefEntityToScene);
    for(a=0;a<6;++a)for(b=0;b<22;++b)for(c=0;c<2;++c)for(d=0;d<3;++d)for(e=0;e<3;++e)for(f=0;f<3;++f)for(g=0;g<2;++g)
        compare(base,&grid,starts[a],ages[b],c,d,e,f,g);
    for(a=0;a<1024;++a)for(b=0;b<35501;b+=113)
        compare(base,&grid,a,b,a%2,a%3,(a/3)%3,(a/9)%3,(a/27)%2);
    unhook(&hooks[1]);unhook(&hooks[0]);FreeLibrary(dll);printf("%d smoke-grenade original comparisons passed.\n",cases);return 0;
}
