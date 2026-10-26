#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_view.c"
#include <windows.h>
#include <stdio.h>
extern vmCvar_t cg_aspectMode;
extern void CG_CalcVrect(void);
static refdef_t captured;
static int calls,fogStart,fogEnd;static float fogValues[4];
static char config[256];
static void noop(void){}
static int dry(const vec3_t p,int pass){return 0;}
static const char *configSink(int n){return config;}
static void sceneSink(const refdef_t *rd){captured=*rd;++calls;}
static void fogSink(int type,int start,int end,float r,float g,float b,float density){fogStart=start;fogEnd=end;fogValues[0]=r;fogValues[1]=g;fogValues[2]=b;fogValues[3]=density;}
static void patch(void *address,void *fn){unsigned char *p=address;DWORD old,tmp;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char **argv){
    HMODULE dll;unsigned char *b;void (__cdecl *parse)(void),(__cdecl *draw)(int),(__cdecl *rect)(void);
    refdef_t input,expected;int i,j,k,expectedCalls,expectedStart,expectedEnd,cases=0;
    float expectedFog[4];int sizes[][2]={{640,480},{800,600},{1280,720},{1280,800},{1920,1080},{1920,1200},{3440,1440}};
    setbuf(stdout,NULL);if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)dll;
    if(sizeof(refdef_t)!=0x1ac)return 2;
    parse=(void *)(b+0x6aff0);draw=(void *)(b+0x6b2a0);rect=(void *)(b+0x6a110);
    patch(b+0x481a0,configSink);patch(CG_ConfigString,configSink);
    patch(b+0x65380,fogSink);patch(trap_R_SetFog,fogSink);
    patch(b+0x653e0,sceneSink);patch(trap_R_RenderScene,sceneSink);
    for(i=0;i<128;++i){
        sprintf(config,"%d %d %d %d %d %d .2 .3 .4 100 900",i-64,i*5,-i*3,i%33,i*7,i%2);
        parse();expectedStart=fogStart;expectedEnd=fogEnd;memcpy(expectedFog,fogValues,sizeof(expectedFog));
        CG_ParseSkyBox();
        if(cg.skyboxEnabled!=*(int *)(b+0x40a2424) || memcmp(cg.skyboxViewOrg,b+0x40a2428,12) ||
           cg.skyboxViewFov!=*(float *)(b+0x40a2434) || cg.tceSkyboxAngle!=*(float *)(b+0x40a4bb8) ||
           expectedStart!=fogStart || expectedEnd!=fogEnd || memcmp(expectedFog,fogValues,sizeof(expectedFog)))return 1;
        ++cases;
        for(j=0;j<8;++j){
            memset(&input,0xa5,sizeof(input));input.x=3;input.y=7;input.width=1280;input.height=720;input.time=i*17;
            input.fov_x=90+i*.1f;input.fov_y=60;input.vieworg[0]=i*57;input.vieworg[1]=-i*23;input.vieworg[2]=i*2;
            AxisClear(input.viewaxis);cg.refdef_current=&input;*(void **)(b+0x40863ac)=&input;
            cg_skybox.integer=*(int *)(b+0x401256c)=j&1;cg.skyboxEnabled=*(int *)(b+0x40a2424)=(j>>1)&1;
            calls=0;draw((j>>2)&1);expectedCalls=calls;expected=captured;
            calls=0;CG_DrawSkyBoxPortal((j>>2)&1);
            if(calls!=expectedCalls || (calls && memcmp(&expected,&captured,sizeof(expected)))){printf("sky mismatch %d %d org %.9g %.9g %.9g vs %.9g %.9g %.9g\n",i,j,captured.vieworg[0],captured.vieworg[1],captured.vieworg[2],expected.vieworg[0],expected.vieworg[1],expected.vieworg[2]);return 1;}
            ++cases;
        }
    }
    config[0]=0;parse();CG_ParseSkyBox();if(cg.skyboxEnabled)return 1;
    for(i=0;i<7;++i)for(j=0;j<3;++j)for(k=0;k<2;++k){
        cg.showGameView=*(int *)(b+0x4081fa8)=0;
        cgs.glconfig.vidWidth=*(int *)(b+0x257e0c8)=sizes[i][0];cgs.glconfig.vidHeight=*(int *)(b+0x257e0cc)=sizes[i][1];
        cg_aspectMode.integer=*(int *)(b+0x401496c)=j;cg_letterbox.integer=*(int *)(b+0x4014a8c)=k;
        rect();CG_CalcVrect();
        if(memcmp(&cg.refdef,b+0x407f004,16)){printf("viewport mismatch %dx%d mode%d letter%d: ours%d,%d,%d,%d original%d,%d,%d,%d\n",sizes[i][0],sizes[i][1],j,k,cg.refdef.x,cg.refdef.y,cg.refdef.width,cg.refdef.height,*(int *)(b+0x407f004),*(int *)(b+0x407f008),*(int *)(b+0x407f00c),*(int *)(b+0x407f010));return 1;}
        ++cases;
    }
    for(i=0;i<7;++i)for(j=0;j<2;++j)for(k=0;k<2;++k){
        void (__cdecl *letter)(float,float,int)=(void *)(b+0x67370);
        extern void CG_Letterbox(float,float,qboolean);
        cgs.glconfig.vidWidth=*(int *)(b+0x257e0c8)=sizes[i][0];
        cgs.glconfig.vidHeight=*(int *)(b+0x257e0cc)=sizes[i][1];
        cg_letterbox.integer=*(int *)(b+0x4014a8c)=j;
        cg.refdef.x=*(int *)(b+0x407f004)=17;cg.refdef.y=*(int *)(b+0x407f008)=23;
        letter(83.25f,77.5f,k);CG_Letterbox(83.25f,77.5f,k);
        if(memcmp(&cg.refdef,b+0x407f004,16))return 1;
        ++cases;
    }
    {
        static snapshot_t snap;
        static unsigned char originalSnapshot[0x28+sizeof(playerState_t)];
        int mode,aspect;
        memset(&snap,0,sizeof(snap));cg.snap=&snap;
        *(void **)(b+0x40352c4)=originalSnapshot;
        cg.predictedPlayerState.pm_type=*(int *)(b+0x407dec4)=PM_INTERMISSION;
        cg.predictedPlayerState.stats[STAT_HEALTH]=*(int *)(b+0x407df90)=100;
        cg.refdef_current=&cg.refdef;*(void **)(b+0x40863ac)=&input;
        patch(b+0x67d00,noop);patch(CG_Zoom,noop);
        patch(b+0x587d0,dry);patch(CG_PointContents,dry);
        for(mode=-1;mode<4;mode++)for(aspect=-1;aspect<4;aspect++)for(i=0;i<7;i++) {
            memset(&input,0,sizeof(input));memset(&cg.refdef,0,sizeof(cg.refdef));
            input.width=cg.refdef.width=sizes[i][0];input.height=cg.refdef.height=sizes[i][1];
            cg_aspectFovMode.integer=*(int *)(b+0x25772ac)=mode;
            cg_aspectMode.integer=*(int *)(b+0x401496c)=aspect;
            ((int (__cdecl *)(void))(b+0x6aaf0))();CG_CalcFov();
            if(fabs(input.fov_x-cg.refdef.fov_x)>.00001 || fabs(input.fov_y-cg.refdef.fov_y)>.00002){printf("aspect FOV mismatch %d/%d: %g/%g vs %g/%g\n",mode,aspect,input.fov_x,input.fov_y,cg.refdef.fov_x,cg.refdef.fov_y);return 1;}
            ++cases;
        }
    }
    printf("%d original sky parser, fog, portal-refdef, viewport and aspect FOV comparisons passed\n",cases);return 0;
}
