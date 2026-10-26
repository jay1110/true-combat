#include "../game/tce_bullet.h"
#ifdef TCE_TEST_PRODUCTION
#include "../game/g_local.h"
extern vec3_t muzzleTrace,right,up;
extern void Bullet_Endpos(gentity_t *,float,vec3_t *);
static gclient_t testClient;
static gentity_t testEntity;
#endif
#include <windows.h>
#include <stdio.h>
#ifndef TCE_TEST_PRODUCTION
void QDECL Com_Printf(const char *fmt,...) {(void)fmt;}
void QDECL Com_Error(int level,const char *fmt,...) {(void)level;fputs(fmt,stderr);exit(2);}
int Q_vsnprintf(char *out,int size,const char *fmt,va_list args) {
    int r;
    #undef _vsnprintf
    r=_vsnprintf(out,size-1,fmt,args);out[size-1]=0;return r;
}

#endif
static unsigned char client[0x16b0],entity[0x57c];
static unsigned rng=573;
static unsigned next(void){return rng=rng*1664525u+1013904223u;}
static float sample(void){return ((int)(next()%65536)-32768)/128.0f;}
int main(int argc,char **argv) {
    HMODULE dll;unsigned char *base;int run,i,w,comparisons=0;
    void (__cdecl *original)(void *,float,float *);
    tce_bulletAim_t aim;tce_weaponDef_t def;
    float actual[3],expected[3];double maxError=0;
    if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;
    base=(unsigned char *)dll;original=(void *)(base+0x9d1a0);
    *(void **)(entity+0x190)=client;
    for(run=0;run<262144;++run) {
        memset(&def,0,sizeof(def));memset(&aim,0,sizeof(aim));
        w=run%64;aim.seed=next();aim.aiming=!!(run&64);
        aim.prone=!!(run&128);aim.ducked=!!(run&256);
        aim.movementSpread=next()%10001;aim.shotSpread=next()%10001;
        aim.phase=(int)(next()%2001)-1000;
        def.unknown_0f8[0]=(int)(next()%2001)-100;
        def.unknown_0f8[3]=(int)(next()%2001)-100;
        def.unknown_0f8[2]=(int)(next()%2001)-100;
        def.scoped=(run&512)?4:0;def.unknown_1c0=!!(run&1024);
        for(i=0;i<3;++i) {
            aim.shotAngles[i]=sample();aim.muzzle[i]=sample();
            aim.right[i]=sample()/256;aim.up[i]=sample()/256;
        }
        memcpy(base+0x24435c0+w*0x1cc,&def,sizeof(def));
        memcpy(base+0x66bb90,aim.muzzle,12);
        memcpy(base+0x66bbb0,aim.right,12);memcpy(base+0x66bbd0,aim.up,12);
        memcpy(client+0x129c,aim.shotAngles,12);
        *(int *)(entity+0xe8)=w;*(unsigned *)(client+0xec)=aim.seed;
        *(int *)(client+0xf0)=aim.aiming?4:0;
        *(int *)(client+0xc)=aim.ducked?1:0;*(int *)(client+0x68)=aim.prone?0x80000:0;
        *(int *)(client+0x390)=aim.movementSpread;*(int *)(client+0x394)=aim.shotSpread;
        *(int *)(client+0x104)=aim.phase;
        original(entity,run%2?9876:-9876,expected);
        TCE_BulletEndpos(&def,&aim,actual);
#ifdef TCE_TEST_PRODUCTION
        if(!aim.aiming && (w==3 || w==8 || w==10 || w==45)) {
            gearDef.parsed=1;weaponDef[w]=def;weaponDef[w].parsed=1;
            memset(&testClient,0,sizeof(testClient));memset(&testEntity,0,sizeof(testEntity));
            testEntity.client=&testClient;testEntity.s.weapon=w;
            testClient.ps.stats[STAT_TCE_SHOT_SEED]=aim.seed;
            testClient.ps.stats[STAT_TCE_AIM_PHASE]=aim.phase;
            testClient.ps.eFlags=aim.prone?EF_PRONE:0;
            testClient.ps.pm_flags=aim.ducked?PMF_DUCKED:0;
            testClient.ps.holdable[0]=aim.movementSpread;testClient.ps.holdable[1]=aim.shotSpread;
            VectorCopy(aim.shotAngles,testClient.pmext.tceShotAngles);
            VectorCopy(aim.muzzle,muzzleTrace);VectorCopy(aim.right,right);VectorCopy(aim.up,up);
            Bullet_Endpos(&testEntity,1234,&actual);
        } else continue;
#endif
        ++comparisons;
        for(i=0;i<3;++i) {
            double e=fabs(actual[i]-expected[i]);if(e>maxError)maxError=e;
            if(e>0.004+fabs(expected[i])*0.000001) {
                fprintf(stderr,"run%d axis%d actual%.9g expected%.9g error%.9g\n",run,i,actual[i],expected[i],e);return 1;
            }
        }
        if(*(unsigned *)(client+0xec)!=aim.seed)return 1;
    }
    printf("%d original Bullet_Endpos comparisons matched; max absolute error %.9g\n",comparisons,maxError);
    FreeLibrary(dll);return 0;
}
