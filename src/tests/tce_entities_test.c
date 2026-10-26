/* Original Windows functions run only in this isolated, hash-pinned process.
 * Dependency hooks are restored before unloading the reference DLL. */
#include "../game/g_local.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setjmp.h>

gentity_t g_entities[MAX_GENTITIES];
void SP_target_environment(gentity_t *ent);
void SP_target_location(gentity_t *ent);
void target_location_linkup(gentity_t *ent);
static jmp_buf entityAbort;
static const char *keys[12], *values[12];
static int keyCount, soundCount, linkCount, configCount;
static char sounds[4][MAX_QPATH], configs[90][128], errorText[256];
static int configIds[90];

qboolean G_SpawnStringExt(const char *key,const char *def,char **out,const char *file,int line) {
    int i; (void)file; (void)line;
    for(i=0;i<keyCount;++i) if(!strcmp(keys[i],key)){*out=(char *)values[i];return qtrue;}
    *out=(char *)def;return qfalse;
}
qboolean G_SpawnFloatExt(const char *key,const char *def,float *out,const char *file,int line) {
    char *value; int found=G_SpawnStringExt(key,def,&value,file,line);*out=(float)atof(value);return found;
}
qboolean G_SpawnIntExt(const char *key,const char *def,int *out,const char *file,int line) {
    char *value; int found=G_SpawnStringExt(key,def,&value,file,line);*out=atoi(value);return found;
}
int G_SoundIndex(const char *name) {
    if(soundCount>=4)exit(1);
    Q_strncpyz(sounds[soundCount],name,MAX_QPATH);return ++soundCount;
}
void trap_LinkEntity(gentity_t *ent) {(void)ent;++linkCount;}
char *vtos(const vec3_t v) {(void)v;return "(0 0 0)";}
void QDECL G_Error(const char *fmt,...) {
    va_list args;va_start(args,fmt);Q_vsnprintf(errorText,sizeof(errorText),fmt,args);va_end(args);
    longjmp(entityAbort,1);
}
void trap_SetConfigstring(int index,const char *value) {
    if(configCount>=90)exit(1);
    configIds[configCount]=index;Q_strncpyz(configs[configCount++],value?value:"",128);
}
void G_SetOrigin(gentity_t *ent,vec3_t origin) {
    VectorCopy(origin,ent->s.pos.trBase);ent->s.pos.trType=TR_STATIONARY;
    ent->s.pos.trTime=ent->s.pos.trDuration=0;VectorClear(ent->s.pos.trDelta);
    VectorCopy(origin,ent->r.currentOrigin);
}
typedef struct {unsigned char *address;unsigned char saved[5];} hook_t;
static void hook(hook_t *h,unsigned char *base,int offset,void *replacement) {
    DWORD protection,unused;h->address=base+offset;memcpy(h->saved,h->address,5);
    if(!VirtualProtect(h->address,5,PAGE_EXECUTE_READWRITE,&protection))exit(1);
    h->address[0]=0xe9;*(int *)(h->address+1)=(unsigned char *)replacement-h->address-5;
    FlushInstructionCache(GetCurrentProcess(),h->address,5);
    VirtualProtect(h->address,5,protection,&unused);
}
static void unhook(hook_t *h) {
    DWORD protection,unused;VirtualProtect(h->address,5,PAGE_EXECUTE_READWRITE,&protection);
    memcpy(h->address,h->saved,5);FlushInstructionCache(GetCurrentProcess(),h->address,5);
    VirtualProtect(h->address,5,protection,&unused);
}
static void require(int ok,const char *what) {if(!ok){fprintf(stderr,"Entity comparison failed: %s\n",what);exit(1);}}
static void compareEnvironment(unsigned char *base,int flags,int count) {
    gentity_t actual;unsigned char reference[0x57c];
    char savedSounds[4][MAX_QPATH],savedError[256];int a,b,savedCount,savedLinks;
    void (__cdecl *original)(void *)=(void *)(base+0x8c010);
    memset(&actual,0,sizeof(actual));memset(reference,0,sizeof(reference));
    actual.spawnflags=flags;actual.count=count;actual.r.svFlags=SVF_PORTAL;
    *(int *)(reference+0x1a8)=flags;*(int *)(reference+0x314)=count;*(int *)(reference+0x128)=SVF_PORTAL;
    soundCount=linkCount=0;errorText[0]=0;a=setjmp(entityAbort);if(!a)SP_target_environment(&actual);
    savedCount=soundCount;savedLinks=linkCount;memcpy(savedSounds,sounds,sizeof(sounds));strcpy(savedError,errorText);
    soundCount=linkCount=0;errorText[0]=0;b=setjmp(entityAbort);if(!b)original(reference);
    require(a==b && !strcmp(savedError,errorText),"environment error");
    require(savedCount==soundCount && savedLinks==linkCount,"environment calls");
    require(!memcmp(savedSounds,sounds,soundCount*MAX_QPATH),"sound paths/order");
    require(!memcmp(&actual,reference,sizeof(sharedEntity_t)),"environment network/shared state");
}
void testServerEntities(unsigned char *base) {
    hook_t hooks[9];int i,j,k,n=0,flags;const char *exposures[]={"0","0.1","0.5","1","-0.1","1.234567"};
    const char *volumes[]={"0","-1","1","255","300"};
    void (__cdecl *originalSpawn)(void *)=(void *)(base+0x8cf50);
    void (__cdecl *originalLink)(void *)=(void *)(base+0x8cea0);
    unsigned char raw[0x57c],*entities=base+0x226e2a0;
    require(sizeof(entityState_t)==0x120 && offsetof(gentity_t,r)==0x120,"shared entity ABI");
    hook(&hooks[0],base,0x85940,G_SpawnStringExt);hook(&hooks[1],base,0x859f0,G_SpawnFloatExt);
    hook(&hooks[2],base,0x85a30,G_SpawnIntExt);hook(&hooks[3],base,0x947f0,G_SoundIndex);
    hook(&hooks[4],base,0x8afa0,trap_LinkEntity);hook(&hooks[5],base,0x5ee10,G_Error);
    hook(&hooks[6],base,0x94cb0,vtos);hook(&hooks[7],base,0x8adc0,trap_SetConfigstring);
    hook(&hooks[8],base,0x951d0,G_SetOrigin);
    keyCount=0;compareEnvironment(base,0,0);++n;
    keys[0]="ambient";values[0]="sound/test";keyCount=1;
    for(flags=0;flags<4;++flags){compareEnvironment(base,flags,0);++n;}
    keys[1]="reverb";values[1]="sound/room";
    keys[2]="eyeadaptation";keys[3]="eyeadaptation_outdoor";keys[4]="eyeadaptation_default";keys[5]="eyeadaptation_sun";
    keys[6]="volume";keyCount=7;
    for(flags=0;flags<4;++flags)for(i=0;i<6;++i)for(j=0;j<5;++j)for(k=-1;k<=1;++k) {
        values[2]=values[3]=values[4]=values[5]=exposures[i];values[6]=volumes[j];
        compareEnvironment(base,flags,k*128);++n;
    }
    memset(&g_entities[0],0,sizeof(g_entities[0]));memset(raw,0,sizeof(raw));
    VectorSet(g_entities[0].s.origin,12.5f,-99.0f,73.25f);
    memcpy(raw+0x5c,g_entities[0].s.origin,sizeof(vec3_t));
    require(ET_ENVIRONMENT==62 && ET_EVENTS==67 && CS_LOCATIONS==943,"TC entity/configstring constants");
    level.time=*(int *)(base+0x7febe0)=12345;
    SP_target_location(&g_entities[0]);originalSpawn(raw);
    require(g_entities[0].nextthink==*(int *)(raw+0x29c),"location nextthink");
    require(g_entities[0].think==target_location_linkup && *(void **)(raw+0x2a4)==originalLink,"location think");
    require(!memcmp(&g_entities[0],raw,sizeof(sharedEntity_t)),"location origin");
    for(k=0;k<4;++k) {
        int savedIds[90],savedCount;char savedConfigs[90][128];gentity_t *head;
        int entitiesToTest = k==3 ? TCE_MAX_LOCATIONS-1 : k?8:0;
        memset(g_entities,0,TCE_MAX_LOCATIONS*sizeof(g_entities[0]));memset(entities,0,TCE_MAX_LOCATIONS*0x57c);
        level.num_entities=*(int *)(base+0x7feb8c)=entitiesToTest;
        level.locationLinked=*(int *)(base+0x2223960)=0;level.locationHead=NULL;
        *(void **)(base+0x2223964)=NULL;
        for(i=0;i<entitiesToTest;++i) {
            char *name=k==3?"target_location":i%3==0?NULL:i%3==1?"target_location":"misc_model";
            if(k==2 && i%3==1)name="TARGET_LOCATION";
            g_entities[i].classname=name;g_entities[i].message="Hall";
            *(char **)(entities+i*0x57c+0x1a4)=name;*(char **)(entities+i*0x57c+0x248)="Hall";
        }
        configCount=0;target_location_linkup(NULL);target_location_linkup(NULL);
        savedCount=configCount;memcpy(savedIds,configIds,sizeof(configIds));memcpy(savedConfigs,configs,sizeof(configs));
        configCount=0;originalLink(NULL);originalLink(NULL);
        require(savedCount==configCount && !memcmp(savedIds,configIds,configCount*sizeof(int)) && !memcmp(savedConfigs,configs,configCount*128),"location configstrings/idempotence");
        require(level.locationLinked==*(int *)(base+0x2223960),"location linked flag");
        head=level.locationHead;
        require(head ? *(void **)(base+0x2223964)==entities+(head-g_entities)*0x57c : !*(void **)(base+0x2223964),"location head");
        for(i=0;i<level.num_entities;++i) {
            gentity_t *next=g_entities[i].nextTrain;
            require(g_entities[i].health==*(int *)(entities+i*0x57c+0x2c8),"location ID");
            require(next ? *(void **)(entities+i*0x57c+0x21c)==entities+(next-g_entities)*0x57c : !*(void **)(entities+i*0x57c+0x21c),"location list");
        }
    }
    level.locationLinked=qfalse;level.num_entities=TCE_MAX_LOCATIONS;configCount=0;
    for(i=0;i<TCE_MAX_LOCATIONS;++i) {g_entities[i].classname="target_location";g_entities[i].message="Hall";}
    i=setjmp(entityAbort);if(!i)target_location_linkup(NULL);
    require(i && configCount==TCE_MAX_LOCATIONS && !strcmp(errorText,"Too many target_location entities"),"location overflow guard");
    for(i=8;i>=0;--i)unhook(&hooks[i]);
    printf("%d environment comparisons plus location spawn/linkup comparisons passed.\n",n);
}
