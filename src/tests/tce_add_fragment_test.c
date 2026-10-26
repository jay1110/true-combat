#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include "../cgame/cg_local.h"
#include "../cgame/tce_add_fragment.h"

cg_t cg; cgs_t cgs;
typedef struct { int kind, args[8]; float values[16]; refEntity_t ref; } event_t;
static event_t events[64], expectedEvents[64];
static int count,cases,originalMode,seed,randomIndex,mode,motion;
static void *owner;
static vec3_t oldOrigin;
static float gravity;
static void require(int ok,const char *why){if(!ok){fprintf(stderr,"case %d mode %d: %s\n",cases,mode,why);exit(1);}}
static event_t *event(int kind){event_t *e;require(count<64,"event overflow");e=&events[count++];memset(e,0,sizeof(*e));e->kind=kind;return e;}
static void fromRaw(localEntity_t *out,const void *in){memset(out,0x35,sizeof(*out));memcpy(out,in,0x6c);memcpy((char *)out+0x6c,(char *)in+0x70,offsetof(localEntity_t,tceFragment)-0x6c);}
static void toRaw(void *out,const localEntity_t *in){memcpy(out,in,0x6c);*(float *)((char *)out+0x6c)=gravity;memcpy((char *)out+0x70,(char *)in+0x6c,offsetof(localEntity_t,tceFragment)-0x6c);}
static void readEntity(localEntity_t *le,void *p){require(p==owner,"entity identity");if(originalMode)fromRaw(le,p);else *le=*(localEntity_t *)p;}
static void writeEntity(void *p,const localEntity_t *le){if(originalMode)toRaw(p,le);else *(localEntity_t *)p=*le;}
int rand(void){event_t *e=event(1);int r=(seed+97*randomIndex++)&32767;e->args[0]=r;return r;}
void trap_S_AddLoopingSound(const vec3_t origin,const vec3_t velocity,sfxHandle_t sound,int volume,int time){event_t *e=event(2);memcpy(e->values,origin,12);memcpy(e->values+3,velocity,12);e->args[0]=sound;e->args[1]=volume;e->args[2]=time;}
void CG_PositionEntityOnTag(refEntity_t *out,const refEntity_t *parent,const char *name,int start,vec3_t *offset){event_t *e=event(3);refEntity_t zero;memset(&zero,0,sizeof(zero));require(!memcmp(out,&zero,sizeof(zero)),"zero smoke entity");require(!strcmp(name,"tag_flash")&&!start&&!offset,"tag arguments");e->ref=*parent;memcpy(e->values,parent->origin,12);out->origin[0]=parent->origin[0]+2;out->origin[1]=parent->origin[1]-3;out->origin[2]=parent->origin[2]+4;}
void CG_ParticleImpactSmokePuffExtended(qhandle_t shader,vec3_t origin,int life,int vel,int acc,int roll,float alpha,float size){event_t *e=event(4);e->args[0]=shader;e->args[1]=life;e->args[2]=vel;e->args[3]=acc;e->args[4]=roll;memcpy(e->values,origin,12);e->values[3]=alpha;e->values[4]=size;}
void trap_R_AddRefEntityToScene(const refEntity_t *re){event(5)->ref=*re;}
void CG_Trace(trace_t *out,const vec3_t start,const vec3_t mins,const vec3_t maxs,const vec3_t end,int skip,int mask){event_t *e=event(6);require(!mins&&!maxs,"point trace");memcpy(e->values,start,12);memcpy(e->values+3,end,12);e->args[0]=skip;e->args[1]=mask;memset(out,0,sizeof(*out));out->fraction=mask==0x10081?(mode==0?.5f:1):(mode<=1?1:.4f);memcpy(out->endpos,end,12);out->endpos[2]-=1;out->plane.normal[2]=1;out->entityNum=1022;}
static void __cdecl evaluate(const trajectory_t *tr,int time,vec3_t out,qboolean angle,int spline,float scale){event_t *e=event(7);e->args[0]=tr->trType;e->args[1]=time;e->args[2]=angle;e->args[3]=spline;e->args[4]=tr->trTime;e->args[5]=tr->trDuration;memcpy(e->values,tr->trBase,12);memcpy(e->values+3,tr->trDelta,12);e->values[6]=scale;if(angle){out[0]=23;out[1]=-37;out[2]=89;}else{memcpy(out,oldOrigin,12);if(motion){out[0]+=5;out[1]-=3;out[2]+=7;}}}
int CG_PointContents(const vec3_t point,int pass){event_t *e=event(8);memcpy(e->values,point,12);e->args[0]=pass;return mode==2?CONTENTS_NODROP:0;}
void CG_FreeLocalEntity(localEntity_t *p){require((void *)p==owner,"free identity");event(9);}
static void bounce(localEntity_t *p,trace_t *trace){localEntity_t le;event_t *e=event(10);readEntity(&le,p);e->args[0]=le.leBounceSoundType;memcpy(e->values,trace->endpos,12);le.leBounceSoundType=LEBS_NONE;writeEntity(p,&le);}
void CG_ReflectVelocity(localEntity_t *p,trace_t *trace){localEntity_t le;event_t *e=event(11);readEntity(&le,p);memcpy(e->values,trace->endpos,12);le.pos.trType=mode==3?TR_STATIONARY:mode==4?TR_GRAVITY_PAUSED:TR_GRAVITY;le.pos.trDelta[0]=motion?32:0;le.pos.trDelta[1]=motion?-47:0;le.pos.trDelta[2]=motion?83:0;writeEntity(p,&le);}
void CG_BloodTrail(localEntity_t *p){localEntity_t le;event(12);readEntity(&le,p);le.headJuncIndex=837;writeEntity(p,&le);}
void CG_FragmentBounceMark(localEntity_t *p,trace_t *trace){localEntity_t le;event_t *e=event(13);readEntity(&le,p);memcpy(e->values,trace->endpos,12);le.leMarkType=LEMT_NONE;writeEntity(p,&le);}
qhandle_t trap_R_GetShaderFromModel(qhandle_t model,int surface,int lightmap){event_t *e=event(14);e->args[0]=model;e->args[1]=surface;e->args[2]=lightmap;return 918;}
void CG_Explodef(vec3_t origin,vec3_t dir,int mass,int type,qhandle_t sound,int low,qhandle_t shader){event_t *e=event(15);memcpy(e->values,origin,12);memcpy(e->values+3,dir,12);e->args[0]=mass;e->args[1]=type;e->args[2]=sound;e->args[3]=low;e->args[4]=shader;}
void QDECL Com_Printf(const char *fmt,...){(void)fmt;}
void QDECL Com_Error(int level,const char *fmt,...){(void)level;fputs(fmt,stderr);exit(2);}
typedef struct {unsigned char *p,saved[5];} hook_t;
static void hook(hook_t *h,unsigned char *base,int off,void *fn){DWORD p,u;h->p=base+off;memcpy(h->saved,h->p,5);require(VirtualProtect(h->p,5,PAGE_EXECUTE_READWRITE,&p),"hook");h->p[0]=0xe9;*(int *)(h->p+1)=(unsigned char *)fn-h->p-5;FlushInstructionCache(GetCurrentProcess(),h->p,5);VirtualProtect(h->p,5,p,&u);}
static void unhook(hook_t *h){DWORD p,u;require(VirtualProtect(h->p,5,PAGE_EXECUTE_READWRITE,&p),"unhook");memcpy(h->p,h->saved,5);FlushInstructionCache(GetCurrentProcess(),h->p,5);VirtualProtect(h->p,5,p,&u);}
static int closeFloat(float a,float b){return _finite(a)&&_finite(b)&&fabs(a-b)<=.0001+fabs(b)*.000003;}
static void compareRef(const refEntity_t *a,const refEntity_t *b){refEntity_t x=*a,y=*b;int i,j;
    for(i=0;i<3;++i){require(closeFloat(x.origin[i],y.origin[i]),"render origin");require(closeFloat(x.fireRiseDir[i],y.fireRiseDir[i]),"flame direction");for(j=0;j<3;++j)require(closeFloat(x.axis[i][j],y.axis[i][j]),"render axis");}
    memcpy(x.origin,y.origin,12);memcpy(x.fireRiseDir,y.fireRiseDir,12);memcpy(x.axis,y.axis,36);
    if(memcmp(&x,&y,sizeof(x)))for(i=0;i<sizeof(x);++i)if(((byte *)&x)[i]!=((byte *)&y)[i]){fprintf(stderr,"ref byte %x: %u vs %u\n",i,((byte *)&x)[i],((byte *)&y)[i]);break;}
    require(!memcmp(&x,&y,sizeof(x)),"complete rendered entity");
}
static void compare(unsigned char *base,int tr,int fire,int variant,int flags,int breaking,int rnd){
    localEntity_t le,expected;unsigned char raw[448];tce_fragmentContext_t context,savedContext;int i,j,n,r;
    void (__cdecl *original)(void *)=(void *)(base+0x450f0);
    memset(&le,0x35,sizeof(le));memset(&context,0,sizeof(context));
    context.sizeVariant=variant&1;context.gravityScale=gravity=variant==0?0:variant==1?1:variant==2?.25f:-.5f;
    context.evaluateTrajectory=evaluate;context.bounceSound=bounce;savedContext=context;
    cg.time=*(int *)(base+0x407dea4)=cases&1?17000001:10000;*(int *)(base+0x3fdf380)=context.sizeVariant;
    le.startTime=cg.time-350;le.endTime=cg.time+(variant==0?5000:variant==1?5001:variant==2?100:10000);
    le.refEntity.fadeStartTime=variant<2?0:cg.time-1;le.refEntity.fadeEndTime=variant==3?le.endTime:le.endTime-1;
    le.onFireStart=fire==0?0:fire==1?cg.time:cg.time-300;
    le.onFireEnd=cg.time+(fire==2?-1:fire==3?0:fire==4?1:500);
    le.pos.trType=tr;le.pos.trTime=cg.time-7;le.pos.trDuration=200;
    le.angles.trType=variant&1?TR_LINEAR:TR_STATIONARY;le.angles.trTime=cg.time-13;le.angles.trDuration=150;
    le.leFlags=flags;le.breakCount=breaking;le.sizeScale=variant==0?0:variant==1?1:variant==2?.875f:1.75f;
    le.leBounceSoundType=variant&1?1:8;le.leMarkType=variant&1?1:0;
    le.refEntity.nonNormalizedAxes=variant&1;le.refEntity.hModel=817;
    for(i=0;i<3;++i){le.refEntity.origin[i]=oldOrigin[i]=(i-1)*35.25f;le.pos.trBase[i]=i*17;le.pos.trDelta[i]=(i-1)*19.125f;le.angles.trBase[i]=(i-1)*31;le.angles.trDelta[i]=(i-1)*57.5f;}
    cgs.media.flameCrackSound=*(int *)(base+0x2588cd4)=101;
    cgs.media.flameSound=*(int *)(base+0x2588cc4)=102;cgs.media.flameBlowSound=*(int *)(base+0x2588cc8)=103;
    cgs.media.onFireShader=*(int *)(base+0x2588504)=201;cgs.media.onFireShader2=*(int *)(base+0x2588508)=202;
    cgs.media.smokeParticleShader=*(int *)(base+0x2588574)=301;
    toRaw(raw,&le);seed=rnd;originalMode=1;owner=raw;count=randomIndex=0;original(raw);
    n=count;r=randomIndex;memcpy(expectedEvents,events,n*sizeof(event_t));fromRaw(&expected,raw);
    require(*(float *)(raw+0x6c)==gravity,"original gravity unchanged");
    originalMode=0;owner=&le;count=randomIndex=0;TCE_CG_AddFragment(&le,&context);
    require(n==count&&r==randomIndex,"call count/RNG");
    for(i=0;i<n;++i){require(events[i].kind==expectedEvents[i].kind,"call order");
        if(memcmp(events[i].args,expectedEvents[i].args,sizeof(events[i].args)))fprintf(stderr,"event %d kind %d\n",i,events[i].kind);
        require(!memcmp(events[i].args,expectedEvents[i].args,sizeof(events[i].args)),"integer arguments");
        for(j=0;j<16;++j)require(closeFloat(events[i].values[j],expectedEvents[i].values[j]),"float arguments");
        compareRef(&events[i].ref,&expectedEvents[i].ref);
    }
    compareRef(&le.refEntity,&expected.refEntity);le.refEntity=expected.refEntity;
    require(closeFloat(le.sizeScale,expected.sizeScale),"size scale");le.sizeScale=expected.sizeScale;
    for(i=0;i<3;++i){require(closeFloat(le.angles.trDelta[i],expected.angles.trDelta[i]),"angular slowing");le.angles.trDelta[i]=expected.angles.trDelta[i];}
    require(!memcmp(&le,&expected,sizeof(le)),"complete local entity");
    require(!memcmp(&context,&savedContext,sizeof(context)),"context mutation");++cases;
}
int main(int argc,char **argv){HMODULE dll;unsigned char *base;hook_t h[15];int a,b,c,d,e,f,g;
    const int types[]={TR_STATIONARY,TR_GRAVITY_PAUSED,TR_GRAVITY};
    const int flags[]={0,2,8,10,16,24,0x200,0x21a};
    const int offsets[]={0x885b6,0x650c0,0x2de20,0x52110,0x65290,0x58420,0x43f0,0x587d0,0x448b0,0x44c10,0x44f20,0x44970,0x44a60,0x65260,0x367c0};
    void *functions[]={rand,trap_S_AddLoopingSound,CG_PositionEntityOnTag,CG_ParticleImpactSmokePuffExtended,trap_R_AddRefEntityToScene,CG_Trace,evaluate,CG_PointContents,CG_FreeLocalEntity,bounce,CG_ReflectVelocity,CG_BloodTrail,CG_FragmentBounceMark,trap_R_GetShaderFromModel,CG_Explodef};
    /* Original payload is 448 bytes with gravity at +0x6c. SDK payload is
     * 444 bytes; its independent adapter provenance and gravity follow it. */
    require(offsetof(localEntity_t,tceFragment)==444&&offsetof(localEntity_t,tceGravity)==448&&sizeof(localEntity_t)==452&&sizeof(refEntity_t)==236,"layout");require(argc==2,"DLL path");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"load");base=(unsigned char *)dll;
    for(a=0;a<15;++a)hook(&h[a],base,offsets[a],functions[a]);
    for(a=0;a<3;++a)for(b=0;b<6;++b)for(c=0;c<4;++c)for(mode=0;mode<6;++mode)for(d=0;d<8;++d)for(e=0;e<3;++e)for(motion=0;motion<2;++motion)for(f=0;f<3;++f)
        compare(base,types[a],b,c,flags[d],e-1,f==0?0:f==1?1:32765);
    for(a=14;a>=0;--a)unhook(&h[a]);FreeLibrary(dll);printf("%d complete AddFragment original comparisons passed.\n",cases);return 0;}
