#include "../cgame/cg_local.h"
#include "../cgame/tce_lightgrid.h"
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
static int cases;
void QDECL Com_Error(int code,const char *fmt,...) {exit(2);}
void QDECL Com_Printf(const char *fmt,...) {}
static void require(int ok,const char *why) {if(!ok){fprintf(stderr,"Light grid case %d: %s\n",cases,why);exit(1);}}
static void vectorEqual(const float *a,const float *b,int n,const char *label) {
    int i;for(i=0;i<n;++i)if(fabs((double)a[i]-b[i]) > 0.00003 + fabs((double)b[i])*0.000003) {
        fprintf(stderr,"%s[%d]: %.9g versus %.9g\n",label,i,a[i],b[i]);require(0,"float comparison");
    }
}
static void compare(unsigned char *base,const tce_lightGrid_t *grid,vec3_t point) {
    float a[11],b[11],color[5],expectedColor[5];vec3_t savedPoint;
    int i;
    void (__cdecl *original)(const float *,float *,float *,float *)=(void *)(base+0x682e0);
    void (__cdecl *simple)(const float *,float *)=(void *)(base+0x68160);
    for(i=0;i<11;++i)a[i]=b[i]=73.25f;
    for(i=0;i<5;++i)color[i]=expectedColor[i]=91.5f;
    VectorCopy(point,savedPoint);
    original(point,a+1,a+4,a+7);
    TCE_CG_LightForPoint(grid,point,b+1,b+4,b+7);
    vectorEqual(b,a,11,"point");
    require(a[0]==73.25f&&a[10]==73.25f&&b[0]==73.25f&&b[10]==73.25f,"output canaries");
    simple(point,expectedColor+1);TCE_CG_LightForParticleSimple(grid,point,color+1);
    vectorEqual(color,expectedColor,5,"simple");
    require(color[0]==91.5f&&color[4]==91.5f&&expectedColor[0]==91.5f&&expectedColor[4]==91.5f,"color canaries");
    require(!memcmp(point,savedPoint,sizeof(vec3_t)),"point modified");
    ++cases;
}
int main(int argc,char **argv) {
    const float positions[]={-2.01f,-2,-1.999f,-1,-0.999f,-0.001f,0,0.001f,0.25f,0.5f,0.99f,1,1.001f,1.5f,1.99f,2,2.5f,2.999f,3,3.99f,4,5};
    unsigned char samples[4*3*5*8],savedSamples[sizeof(samples)];float sine[1024],savedSine[1024];
    tce_lightGrid_t grid,savedGrid;HMODULE dll;unsigned char *base;vec3_t point;
    unsigned int random=9471;int i,j,a,b,c,pattern,shape;
    require(argc==2,"DLL argument");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"load DLL");base=(unsigned char *)dll;
    grid.samples=samples;grid.sine=sine;
    for(i=0;i<1024;++i)sine[i]=(float)sin(i*2.0*3.14159265358979323846/1024.0);
    memcpy(base+0x4845e64,sine,sizeof(sine));memcpy(savedSine,sine,sizeof(sine));
    for(shape=0;shape<2;++shape) {
        VectorSet(grid.bounds,4,3,5);VectorSet(grid.spacing,shape?37.3f:64,shape?61.1f:64,shape?99.7f:128);
        VectorSet(grid.origin,shape?-129.75f:0,shape?100.25f:0,shape?-7.5f:0);
        memcpy(base+0x40a4c24,grid.spacing,12);memcpy(base+0x40a4c30,grid.bounds,12);memcpy(base+0x40a4c58,grid.origin,12);
        for(pattern=0;pattern<6;++pattern) {
            for(i=0;i<sizeof(samples);++i){random=random*1664525u+1013904223u;samples[i]=(byte)(random>>24);}
            for(i=0;i<60;++i) {
                if(pattern==0 || (pattern==2 && i%2) || (pattern==3 && i%3))memset(samples+i*8,0,3);
                if(pattern==4)memset(samples+i*8,255,6);
                if(pattern==5){memset(samples+i*8,1,6);samples[i*8+6]=samples[i*8+7]=0;}
            }
            memcpy(base+0x40a4c64,samples,sizeof(samples));memcpy(savedSamples,samples,sizeof(samples));savedGrid=grid;
            for(a=0;a<22;++a)for(b=0;b<22;++b)for(c=0;c<22;++c) {
                point[0]=grid.origin[0]+positions[a]*grid.spacing[0];
                point[1]=grid.origin[1]+positions[b]*grid.spacing[1];
                point[2]=grid.origin[2]+positions[c]*grid.spacing[2];
                compare(base,&grid,point);
            }
            require(!memcmp(samples,savedSamples,sizeof(samples))&&!memcmp(base+0x40a4c64,savedSamples,sizeof(samples)),"samples modified");
            require(!memcmp(&grid,&savedGrid,sizeof(grid)),"context modified");
        }
    }
    require(!memcmp(sine,savedSine,sizeof(sine))&&!memcmp(base+0x4845e64,savedSine,sizeof(sine)),"sine table modified");
    FreeLibrary(dll);printf("%d light-point and %d particle-light original comparisons passed.\n",cases,cases);return 0;
}
