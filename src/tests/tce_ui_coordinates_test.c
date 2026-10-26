#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../ui/tce_ui_coordinates.h"
int main(int argc,char **argv) {
    HMODULE dll;unsigned char *base,*dc;int mode,res,n,alias,i,cases=0;
    int sizes[][2]={{640,480},{800,600},{1024,768},{1280,720},{1920,1080},{1280,800},{2560,1080},{3440,1440},{720,1280}};
    void (__cdecl *fn)(float *,float *,float *,float *);
    if(argc!=2||(dll=LoadLibraryA(argv[1]))==NULL)return 2;
    base=(unsigned char *)dll;dc=calloc(1,0x20100);if(!dc)return 3;
    *(void **)(base+0x245fa4)=dc;
    for(mode=0;mode<2;++mode)for(res=0;res<9;++res)for(n=0;n<257;++n)for(alias=0;alias<4;++alias) {
        float a[4],b[4],xs=(float)(sizes[res][0]/640.0),ys=(float)(sizes[res][1]/480.0);
        int ids[4]={0,1,2,3};
        if(alias==1)ids[1]=0;if(alias==2){ids[2]=0;ids[3]=1;}if(alias==3)ids[1]=ids[2]=ids[3]=0;
        a[0]=(n-20)*3.375f;a[1]=(n-30)*2.125f;a[2]=n*.75f;a[3]=n*.25f;memcpy(b,a,sizeof(a));
        *(int *)(base+0x2b2028)=*(int *)(dc+0x20008)=sizes[res][0];
        *(int *)(base+0x2b202c)=*(int *)(dc+0x2000c)=sizes[res][1];
        *(float *)(base+0x292138)=*(float *)(dc+0x118)=xs;
        *(float *)(base+0x292134)=*(float *)(dc+0x114)=ys;
        fn=(void *)(base+(mode?0x1c370:0x2600));fn(a+ids[0],a+ids[1],a+ids[2],a+ids[3]);
        TCE_UI_AdjustCoordinates(b+ids[0],b+ids[1],b+ids[2],b+ids[3],sizes[res][0],sizes[res][1],xs,ys);
        for(i=0;i<4;++i)if(fabs(a[i]-b[i])>0.0005f){fprintf(stderr,"mode%d res%d n%d alias%d component%d: %.9g / %.9g\n",mode,res,n,alias,i,a[i],b[i]);return 1;}
        ++cases;
    }
    free(dc);FreeLibrary(dll);printf("%d original UI coordinate comparisons passed (two functions).\n",cases);return 0;
}
