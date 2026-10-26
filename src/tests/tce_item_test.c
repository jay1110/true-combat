#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include "../cgame/cg_local.h"
cg_t cg; cgs_t cgs; weaponInfo_t cg_weapons[MAX_WEAPONS]; itemInfo_t cg_items[MAX_ITEMS];
gitem_t bg_itemlist[128]; int bg_numItems=128; vmCvar_t cg_drawCrosshairPickups;
static snapshot_t snap;
static int cases, count, seen;
typedef struct { int kind,a,b; float data[16]; refEntity_t ent; } call_t;
static call_t calls[8],expected[8];
void TCE_CG_Item(centity_t *cent);
static void check(int ok,const char *s){if(!ok){fprintf(stderr,"item case %d: %s\n",cases,s);exit(1);}}
static call_t *record(int kind){call_t *c=&calls[count++];check(count<8,"calls");memset(c,0,sizeof(*c));c->kind=kind;return c;}
void QDECL CG_Error(const char *s,...){fprintf(stderr,"unexpected item error %s\n",s);exit(1);}
char *QDECL va(char *fmt,...){static char buf[128];va_list ap;va_start(ap,fmt);vsprintf(buf,fmt,ap);va_end(ap);return buf;}
qboolean CG_PlayerSeesItem(playerState_t *ps,entityState_t *es,int time,int type){call_t *c=record(1);c->a=time;c->b=type;return seen;}
void trap_R_ModelBounds(qhandle_t model,vec3_t mins,vec3_t maxs){call_t *c=record(2);c->a=model;VectorSet(mins,-3,-7,-11);VectorSet(maxs,13,17,23);}
void trap_R_AddRefEntityToScene(const refEntity_t *ent){record(3)->ent=*ent;}
void trap_R_AddCoronaToScene(const vec3_t origin,float r,float g,float b,float scale,int id,qboolean visible){call_t *c=record(4);VectorCopy(origin,c->data);c->data[3]=r;c->data[4]=g;c->data[5]=b;c->data[6]=scale;c->a=id;c->b=visible;}
void CG_PositionEntityOnTag(refEntity_t *ent,const refEntity_t *parent,const char *tagName,int startIndex,vec3_t *offset){call_t *c=record(5);c->ent=*parent;c->a=startIndex;c->b=!strcmp(tagName,"tag_stand")?0:atoi(tagName+9);check(!offset,"tag offset");memcpy(ent->axis,parent->axis,sizeof(ent->axis));VectorCopy(parent->origin,ent->origin);ent->origin[2]+=3;}
static void patch(unsigned char *p,void *fn){DWORD old,tmp;check(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old),"protect");p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static void floats(const float *a,const float *b,int n){int i;for(i=0;i<n;++i)if(!(_finite(a[i])&&_finite(b[i])&&fabs(a[i]-b[i])<.0001f+fabs(b[i])*.000003f)){fprintf(stderr,"float index%d source%.9g original%.9g\n",i,a[i],b[i]);check(0,"float result");}}
int main(int argc,char **argv){
    HMODULE dll; unsigned char *base; void (__cdecl *original)(void *); int k,j,expectedCount;
    static unsigned char raw[4096],rawSnap[4096]; centity_t cent; refEntity_t a,b;
    if(argc==2&&!strcmp(argv[1],"--offsets")){printf("{\"stride\":%u,\"valid\":%u,\"origin\":%u}\n",(unsigned)sizeof(centity_t),(unsigned)offsetof(centity_t,currentValid),(unsigned)offsetof(centity_t,lerpOrigin));return 0;}check(argc==2,"DLL path");dll=LoadLibraryA(argv[1]);check(dll!=NULL,"load");base=(unsigned char *)dll;original=(void *)(base+0x30080);
    patch(base+0x65290,trap_R_AddRefEntityToScene);patch(base+0x654f0,trap_R_ModelBounds);patch(base+0x65350,trap_R_AddCoronaToScene);
    patch(base+0x2e290,CG_PlayerSeesItem);patch(base+0x2de20,CG_PositionEntityOnTag);patch(base+0x7e130,AnglesToAxis);patch(base+0x7e1b0,AxisCopy);
    *(int *)(base+0x9a560)=128;*(void **)(base+0x40352c4)=rawSnap;cg.snap=&snap;snap.ps.clientNum=7;*(int *)(rawSnap+0x148)=7;
    for(k=0;k<80000;++k){
        int type=(k/37)%8,weapon=1+k%63,item=k%89==0?0:1+k%66,flags=(k/13)%4,stand=(k/7)%2,pickup=(k/5)%2;
        unsigned char *ow=base+0x484a080+weapon*0xfd0,*oi=base+0x98f58+item*60;
        weaponInfo_t *w=&cg_weapons[weapon];
        memset(&cent,0,sizeof(cent));memset(raw,0,sizeof(raw));memset(w,0,sizeof(*w));memset(ow,0,0xfd0);memset(oi,0,60);
        cent.currentState.modelindex=item;cent.currentState.modelindex2=k%11==0?3:0;cent.currentState.eFlags=(flags&1?EF_SPINNING:0)|(flags&2?EF_NODRAW:0);
        cent.currentState.groundEntityNum=k%3-1;cent.currentState.frame=k%4;cent.currentState.density=k%3;cent.currentState.number=49;
        cent.currentState.otherEntityNum=k%2?254:0;cent.currentState.effect3Time=k%3?254:0;cent.currentState.otherEntityNum2=k%2?7:8;
        VectorSet(cent.lerpOrigin,11,23,37);VectorSet(cent.lerpAngles,17,31,43);VectorSet(cent.lastLerpAngles,29,41,53);
        VectorSet(cent.highlightOrigin,59,61,67);cent.usehighlightOrigin=k%2;
        VectorSet(cg.autoAnglesSlow,7,13,19);AnglesToAxis(cg.autoAnglesSlow,cg.autoAxisSlow);cg.time=k*3;
        w->standModel=stand?107:0;w->droppedAnglesHack=k%2;w->weaponModel[W_PU_MODEL].model=pickup?201:0;w->weaponModel[W_TP_MODEL].model=301;w->modModels[0]=401;
        *(int *)(ow+0x5b8)=w->standModel;*(int *)(ow+0x5bc)=w->droppedAnglesHack;*(int *)(ow+0x5e0)=w->weaponModel[W_PU_MODEL].model;*(int *)(ow+0x5c0)=301;*(int *)(ow+0xe30)=401;
        bg_itemlist[item].giType=type;bg_itemlist[item].giTag=weapon;*(int *)(oi+0x24)=type;*(int *)(oi+0x28)=weapon;
        cg_items[item].models[0]=501;*(int *)(base+0x48478a4+item*32)=501;cgs.gameModels[3]=601;*(int *)(base+0x257e54c+12)=601;
        cg_drawCrosshairPickups.integer=(k/3)%3;*(int *)(base+0x401070c)=cg_drawCrosshairPickups.integer;seen=k%2;
        memcpy(raw,&cent.currentState,sizeof(entityState_t));memcpy(raw+0x710,cent.lerpOrigin,12);memcpy(raw+0x71c,cent.lerpAngles,12);memcpy(raw+0x728,cent.lastLerpAngles,12);memcpy(raw+0x808,cent.highlightOrigin,12);*(int *)(raw+0x814)=cent.usehighlightOrigin;
        memcpy(base+0x407ef74,cg.autoAnglesSlow,12);memcpy(base+0x407ef80,cg.autoAxisSlow,36);*(int *)(base+0x407dea4)=cg.time;
        count=0;memset(calls,0,sizeof(calls));original(raw);expectedCount=count;memcpy(expected,calls,sizeof(calls));
        count=0;memset(calls,0,sizeof(calls));TCE_CG_Item(&cent);check(count==expectedCount,"call count");
        for(j=0;j<count;++j){check(calls[j].kind==expected[j].kind&&calls[j].a==expected[j].a&&calls[j].b==expected[j].b,"callback arguments");floats(calls[j].data,expected[j].data,16);
            a=calls[j].ent;b=expected[j].ent;floats(a.axis[0],b.axis[0],9);floats(a.origin,b.origin,3);floats(a.oldorigin,b.oldorigin,3);
            memset(a.axis,0,sizeof(a.axis));memset(b.axis,0,sizeof(b.axis));memset(a.origin,0,12);memset(b.origin,0,12);memset(a.oldorigin,0,12);memset(b.oldorigin,0,12);check(!memcmp(&a,&b,sizeof(a)),"renderer fields");}
        floats(cent.lerpAngles,(float *)(raw+0x71c),3);floats(cent.lastLerpAngles,(float *)(raw+0x728),3);floats(cent.highlightOrigin,(float *)(raw+0x808),3);check(cent.usehighlightOrigin==*(int *)(raw+0x814),"highlight cache");++cases;
    }
    printf("%d whole CG_Item original comparisons passed\n",cases);FreeLibrary(dll);return 0;
}
