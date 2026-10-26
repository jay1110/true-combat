#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern void SP_worldspawn(void);
static char calls[8192];static int fog,custom;
static void patch(void *at,void *fn){DWORD old,tmp;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static void set(const char *n,const char *v){strcat(calls,n);strcat(calls,"=");strcat(calls,v);strcat(calls,";");}
static void config(int i,const char *v){char key[32];sprintf(key,"config%d",i);
 /* Intentional reconstructed module identity; do not advertise SDK identity. */
 if(i==CS_GAME_VERSION){if(strcmp(v,"Enemy Territory")&&strcmp(v,GAME_VERSION))exit(4);v="<module identity>";}set(key,v);}
static void print(const char *s,...){set("print",s);}
static void match(void){set("match","init");}
static void locations(vec2_t a,vec2_t b){char value[128];sprintf(value,"%g,%g,%g,%g",a[0],a[1],b[0],b[1]);set("locations",value);}
static qboolean spawn(const char *key,const char *def,char **out,const char *file,int line){
 *out=(char *)def;
 if(!strcmp(key,"classname"))*out="worldspawn";
 else if(!strcmp(key,"music"))*out="music.wav";
 else if(!strcmp(key,"message"))*out="TC map";
 else if(!strcmp(key,"enableFog"))*out=fog?"1":"0";
 else if(!strcmp(key,"spawnflags"))*out="5";
 else if(!strcmp(key,"cclayers"))*out=custom?"1":"0";
 else if(custom&&!strcmp(key,"mapcoordsmins"))*out="-1200 1700";
 else if(custom&&!strcmp(key,"mapcoordsmaxs"))*out="1400 -1900";
 else if(custom&&!strcmp(key,"fogNear"))*out="123.5";
 else if(custom&&!strcmp(key,"fogFar"))*out="6789";
 else if(custom&&!strcmp(key,"fogR"))*out="0.12";
 else if(custom&&!strcmp(key,"fogG"))*out="0.23";
 else if(custom&&!strcmp(key,"fogB"))*out="0.34";
 else return qfalse;
 return qtrue;
}
int main(int argc,char **argv){
 HMODULE dll;unsigned char *b;char expected[8192];int m,r,g,n=0,valid,cc,warm;
 char *maps[]={"obj_delta","OBJ_DELTA","obj_stadtrand","obj_northport","dem_northport","dem_railhouse","dem_village","custom"};
 void (*original)(void);if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)dll;original=(void *)(b+0x86170);
 patch(b+0x85940,spawn);patch(G_SpawnStringExt,spawn);patch(b+0x8ac90,set);patch(trap_Cvar_Set,set);
 patch(b+0x8adc0,config);patch(trap_SetConfigstring,config);patch(b+0x2e9d0,locations);patch(BG_InitLocations,locations);
 patch(b+0x5ed80,print);patch(G_Printf,print);patch(b+0x64ef0,match);patch(G_initMatch,match);
 for(m=0;m<8;++m)for(fog=0;fog<2;++fog)for(custom=0;custom<2;++custom)for(r=0;r<2;++r)for(g=0;g<2;++g){
  strcpy((char *)b+0x7feb98,maps[m]);*(int *)(b+0x7febec)=1234;strcpy((char *)b+0x23ce030,"motd");
  *(int *)(b+0x2223aac)=r;*(int *)(b+0x784f6c)=g;*(int *)(b+0x221bae8)=0;*(int *)(b+0x7feb90)=99;memset(b+0x221bc18,0,16);
  calls[0]=0;original();strcpy(expected,calls);valid=*(int *)(b+0x221bc10);cc=*(int *)(b+0x221bae8);warm=*(int *)(b+0x7feb90);
  memset(&level,0,sizeof(level));strcpy(level.rawmapname,maps[m]);level.startTime=1234;level.warmupTime=99;
  strcpy(g_motd.string,"motd");g_restarted.integer=r;g_gamestate.integer=g;
  calls[0]=0;SP_worldspawn();
  if(strcmp(expected,calls)||valid!=level.mapcoordsValid||cc!=level.ccLayers||warm!=level.warmupTime||g_entities[ENTITYNUM_WORLD].spawnflags!=5||g_entities[ENTITYNUM_WORLD].r.worldflags!=5){fprintf(stderr,"worldspawn mismatch map %s fog%d custom%d restart%d state%d\n%s\n%s\n",maps[m],fog,custom,r,g,expected,calls);return 1;}++n;
 }
 printf("%d full worldspawn comparisons matched original DLL\n",n);return 0;
}
