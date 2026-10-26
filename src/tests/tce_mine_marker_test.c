#include "../cgame/cg_local.h"
#include "../cgame/tce_weapon_media.h"
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>

cg_t cg;
static int cases;
static void require(int ok,const char *what) {
    if(!ok){fprintf(stderr,"Mine marker case %d: %s\n",cases,what);exit(1);}
}
static void compare(unsigned char *base,int time,int frameTime,int oldTime,int frame,
                    int offset,int faction,int sdk) {
    static centity_t cent,expectedCent;
    static refEntity_t render,expectedRender;
    static tce_weaponInfo_t media,savedMedia;
    static weaponInfo_t sdkMedia,savedSdkMedia;
    static unsigned char rawCent[2048],rawRender[256],expectedRawCent[2048],expectedRawRender[256];
    void (__cdecl *original)(void *,void *,void *)=(void *)(base+0x31b70);
    memset(&cent,0x5a,sizeof(cent));memset(&render,0x5a,sizeof(render));
    memset(rawCent,0x5a,sizeof(rawCent));memset(rawRender,0x5a,sizeof(rawRender));
    memset(&media,0x5a,sizeof(media));memset(&sdkMedia,0x5a,sizeof(sdkMedia));
    cent.currentState.otherEntityNum2=faction;cent.currentState.frame=offset;
    cent.lerpFrame.frameTime=frameTime;cent.lerpFrame.oldFrameTime=oldTime;
    cent.lerpFrame.frame=frame;cent.lerpFrame.oldFrame=frame+3;
    cent.lerpFrame.backlerp=-13.0f;
    render.origin[2]=-17.5f;render.oldorigin[2]=3.75f;
    media.modModels[0]=sdkMedia.modModels[0]=sdk?0:173;
    media.modModels[1]=sdkMedia.modModels[1]=sdk?-7:291;
    savedMedia=media;savedSdkMedia=sdkMedia;
    *(int *)(rawCent+0x90)=faction;*(int *)(rawCent+0xb0)=offset;
    *(int *)(rawCent+0x7b4)=frame+3;*(int *)(rawCent+0x7b8)=oldTime;
    *(int *)(rawCent+0x7c0)=frame;*(int *)(rawCent+0x7c4)=frameTime;
    *(float *)(rawCent+0x7cc)=-13.0f;
    *(float *)(rawRender+0x70)=render.origin[2];*(float *)(rawRender+0x8c)=render.oldorigin[2];
    expectedCent=cent;expectedRender=render;
    memcpy(expectedRawCent,rawCent,sizeof(rawCent));memcpy(expectedRawRender,rawRender,sizeof(rawRender));
    cg.time=*(int *)(base+0x407dea4)=time;
    original(rawCent,rawRender,&media);
    CG_DrawMineMarkerFlag(&cent,&render,sdk?sdkMedia.modModels:media.modModels);
    require(!memcmp(&media,&savedMedia,sizeof(media)) && !memcmp(&sdkMedia,&savedSdkMedia,sizeof(sdkMedia)),"media modified");
    require(fabs((double)cent.lerpFrame.backlerp-*(float *)(rawCent+0x7cc))<0.000002,"state backlerp");
    require(fabs((double)render.backlerp-*(float *)(rawRender+0xa0))<0.000002,"render backlerp");
#define MAP_STATE(field,off) expectedCent.lerpFrame.field=*(int *)(rawCent+(off)); *(int *)(expectedRawCent+(off))=cent.lerpFrame.field
    MAP_STATE(oldFrame,0x7b4);MAP_STATE(oldFrameTime,0x7b8);
    MAP_STATE(frame,0x7c0);MAP_STATE(frameTime,0x7c4);
#undef MAP_STATE
    expectedCent.lerpFrame.backlerp=cent.lerpFrame.backlerp;
    *(float *)(expectedRawCent+0x7cc)=*(float *)(rawCent+0x7cc);
    expectedRender.hModel=*(int *)(rawRender+8);*(int *)(expectedRawRender+8)=render.hModel;
    expectedRender.frame=*(int *)(rawRender+0x74);*(int *)(expectedRawRender+0x74)=render.frame;
    expectedRender.oldframe=*(int *)(rawRender+0x90);*(int *)(expectedRawRender+0x90)=render.oldframe;
    expectedRender.origin[2]=*(float *)(rawRender+0x70);*(float *)(expectedRawRender+0x70)=render.origin[2];
    expectedRender.oldorigin[2]=*(float *)(rawRender+0x8c);*(float *)(expectedRawRender+0x8c)=render.oldorigin[2];
    expectedRender.backlerp=render.backlerp;*(float *)(expectedRawRender+0xa0)=*(float *)(rawRender+0xa0);
    require(!memcmp(&cent,&expectedCent,sizeof(cent)),"SDK frame state/extra writes");
    require(!memcmp(&render,&expectedRender,sizeof(render)),"SDK render state/extra writes");
    require(!memcmp(rawCent,expectedRawCent,sizeof(rawCent)),"original state/extra writes");
    require(!memcmp(rawRender,expectedRawRender,sizeof(rawRender)),"original render/extra writes");
    ++cases;
}
int main(int argc,char **argv) {
    const int times[]={-101,-1,0,49,50,51,1000,1001,1999};
    const int frameTimes[]={-100,0,50,1001,100001},frames[]={-1,0,18,19,20,37};
    const int offsets[]={-21,-1,0,1,19,20,39,40},factions[]={0,1,-1};
    int a,b,c,d,e,f,g;HMODULE dll;unsigned char *base;
    require(argc==2,"DLL argument");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"load DLL");base=(unsigned char *)dll;
    for(a=0;a<9;++a)for(b=0;b<5;++b)for(c=0;c<3;++c)for(d=0;d<6;++d)
    for(e=0;e<8;++e)for(f=0;f<3;++f)for(g=0;g<2;++g)
        compare(base,times[a],frameTimes[b],c==0?frameTimes[b]:c==1?frameTimes[b]-50:frameTimes[b]+50,
                frames[d],offsets[e],factions[f],g);
    FreeLibrary(dll);printf("%d mine-marker original comparisons passed.\n",cases);return 0;
}
