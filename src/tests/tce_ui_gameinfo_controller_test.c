#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_main.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
static char buffer[4096];static int missing,loads;
static char* load(const char*name){assert(!strcmp(name,"gameinfo.txt"));++loads;return missing?NULL:buffer;}
static unsigned char*originalBase;static int originalMode,arenaCount;
static void arenas(void){++loads;if(originalMode)*(int*)(originalBase+0x2b52a8)=arenaCount;else uiInfo.mapCount=arenaCount;}
static struct{unsigned char*p,bytes[5];}hooks[4];static int hc;
static void patch(void*a,void*f){unsigned char*p=a;DWORD o,t;assert(hc<4);hooks[hc].p=p;memcpy(hooks[hc++].bytes,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].bytes,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}


int main(int argc,char**argv){HMODULE h;unsigned char*b;int k,seed,j,cases=0;
const char*inputs[]={"","}","}suffix gametypes { }","unknown tokens","gametypes { }","GAMETYPES { { 2 Reinforced RO desc } { 5 Objective OBJ desc } { 7 Bodycount BC desc } }","//comment\ngametypes { { 5 \"Objective mode\" OBJ \"Bomb objectives\" } }","gametypes bad gametypes { }","gametypes {","gametypes { { 2 } }","gametypes { { 2 Name } }","gametypes { { 2 Name Short } }","gametypes { { 2 Name Short Desc } } gametypes { { 7 BC BC Last } }","ignored { gametypes { { 5 OBJ OBJ Bomb } } }","gametypes { { 2 N S D } } } gametypes { }","joinGameTypes { ignored } gametypes { }"};
assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;originalBase=b;patch(b+0x4e10,load);patch(GetMenuBuffer,load);patch(b+0x2820,arenas);patch(UI_LoadArenas,arenas);
for(missing=0;missing<2;++missing)for(seed=0;seed<2;++seed)for(k=0;k<16;++k){Init_Display(NULL);*(void**)(b+0x245fa4)=NULL;String_Init();((void(__cdecl*)(void))(b+0x14af0))();memset(uiInfo.gameTypes,0,sizeof(uiInfo.gameTypes));memset(b+0x2b3284,0,256);uiInfo.numGameTypes=*(int*)(b+0x2b3280)=seed?9:0;uiInfo.numJoinGameTypes=*(int*)(b+0x2b3384)=13;
strcpy(buffer,inputs[k]);loads=0;((void(__cdecl*)(const char*))(b+0xd980))("gameinfo.txt");assert(loads==1);strcpy(buffer,inputs[k]);loads=0;UI_ParseGameInfo("gameinfo.txt");assert(loads==1);assert(uiInfo.numGameTypes==*(int*)(b+0x2b3280));assert(uiInfo.numJoinGameTypes==13&&*(int*)(b+0x2b3384)==13);
for(j=0;j<16;++j){gameTypeInfo*row=uiInfo.gameTypes+j;unsigned char*o=b+0x2b3284+16*j;const char*names[]={row->gameType,row->gameTypeShort,row->gameTypeDescription};int f,offs[]={0,4,12};assert(row->gtEnum==*(int*)(o+8));for(f=0;f<3;++f){const char*t=*(char**)(o+offs[f]);assert((t==NULL)==(names[f]==NULL));if(t)assert(!strcmp(t,names[f]));}}++cases;}
{const char*names[]={"obj_delta","obj_northport","OBJ_DELTA","","obj_snow"};const char*queries[]={"obj_delta","OBJ_NORTHPORT","obj_snow","missing","","obj_delta_extra"};int n,q,lazy;for(j=0;j<5;++j){uiInfo.mapList[j].mapLoadName=names[j];*(const char**)(b+0x2b52b0+0x88*j)=names[j];}for(n=0;n<=5;++n)for(lazy=0;lazy<2;++lazy)for(q=0;q<6;++q){void*op;mapInfo*sp;int oi,si,expectedLoads;arenaCount=n;uiInfo.mapCount=*(int*)(b+0x2b52a8)=lazy?0:n;originalMode=1;loads=0;op=((void*(__cdecl*)(const char*))(b+0x2d80))(queries[q]);expectedLoads=loads;oi=op?(int)(((unsigned char*)op-(b+0x2b52ac))/0x88):-1;originalMode=0;loads=0;sp=UI_FindMapInfoByMapname(queries[q]);si=sp?(int)(sp-uiInfo.mapList):-1;assert(si==oi&&loads==expectedLoads&&uiInfo.mapCount==*(int*)(b+0x2b52a8));++cases;}}
restore();assert(FreeLibrary(h));printf("%d original game-info/map-lookup comparisons passed\n",cases);return 0;}
