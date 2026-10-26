#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_main.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
static char trace[2048];static int counts,change;
static int countmaps(qboolean single){assert(single);strcat(trace,"count;");return counts++?3+change:3;}
static void set(const char*n,const char*v){sprintf(trace+strlen(trace),"set:%s=%s;",n,v);}
static void best(const char*m,int g){sprintf(trace+strlen(trace),"best:%s:%d;",m,g);}
static void feeder(menuDef_t*m,int f,int i,const char*n){assert(!m&&!n&&f==1&&!i);strcat(trace,"feeder;");}
static struct {unsigned char*p,bytes[5];} hooks[8];static int hookCount;
static void patch(void*at,void*fn){unsigned char*p=at;DWORD o,t;assert(hookCount<8);hooks[hookCount].p=p;memcpy(hooks[hookCount++].bytes,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hookCount){--hookCount;assert(VirtualProtect(hooks[hookCount].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hookCount].p,hooks[hookCount].bytes,5);FlushInstructionCache(GetCurrentProcess(),hooks[hookCount].p,5);VirtualProtect(hooks[hookCount].p,5,o,&t);}}
int main(int argc,char**argv){HMODULE h;unsigned char*b;int n,current,key,reset,j,cases=0;int keys[]={0,13,169,178,179,180,255};assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;
patch(b+0xb360,countmaps);patch(UI_MapCountByGameType,countmaps);patch(b+0x20530,set);patch(trap_Cvar_Set,set);patch(b+0x16c30,best);patch(UI_LoadBestScores,best);patch(b+0x1d8f0,feeder);patch(Menu_SetFeederSelection,feeder);
ui_currentMap.integer=*(int*)(b+0x2913cc)=0;uiInfo.mapList[0].mapLoadName="obj_northport";*(char**)(b+0x2b52b0)="obj_northport";for(j=0;j<16;++j)uiInfo.gameTypes[j].gtEnum=*(int*)(b+0x2b328c+j*16)=j;
for(n=4;n<=11;++n)for(current=1;current<n;++current)for(key=0;key<7;++key)for(reset=0;reset<2;++reset)for(change=-1;change<=1;++change){int r,selected;char expected[2048];uiInfo.numGameTypes=*(int*)(b+0x2b3280)=n;ui_gameType.integer=*(int*)(b+0x2d926c)=current;counts=0;trace[0]=0;r=((int(__cdecl*)(int,void*,int,int))(b+0xabe0))(0,NULL,keys[key],reset);selected=*(int*)(b+0x2d926c);strcpy(expected,trace);counts=0;trace[0]=0;assert(UI_GameType_HandleKey(0,NULL,keys[key],reset)==r);assert(ui_gameType.integer==selected&&!strcmp(trace,expected));++cases;}
/* Real COM_ParseExt/Int_Parse/String_Parse/String_Alloc; no parser hooks. */
{const char*inputs[]={"","bad","{ }","{","{ garbage }","{ { 2 Reinforced RO desc } { 5 Objective OBJ desc } { 7 Bodycount BC desc } }","{ { 2 } }","{ { 2 Name } }","{ { 2 Name Short } }","{ { 2 Name Short Desc X }","{ { Name 2 } { Name2 5 } }","{ { Name } }","{ { \"\" 7 } }","{ //comment\n { 5 \"Objective mode\" OBJ \"long description\" } }"};int join,k;
for(join=0;join<2;++join)for(k=0;k<14;++k){char text[2048],*op,*sp;int r,num;strcpy(text,inputs[k]);op=sp=text;Init_Display(NULL);*(void**)(b+0x245fa4)=NULL;String_Init();((void(__cdecl*)(void))(b+0x14af0))();memset(uiInfo.gameTypes,0,sizeof(uiInfo.gameTypes));memset(uiInfo.joinGameTypes,0,sizeof(uiInfo.joinGameTypes));memset(b+0x2b3284,0,256);memset(b+0x2b3388,0,256);uiInfo.numGameTypes=*(int*)(b+0x2b3280)=9;uiInfo.numJoinGameTypes=*(int*)(b+0x2b3384)=9;r=((int(__cdecl*)(char**,int))(b+0xd9f0))(&op,join);assert(GameType_Parse(&sp,join)==r&&op==sp);num=join?uiInfo.numJoinGameTypes:uiInfo.numGameTypes;assert(num==*(int*)(b+(join?0x2b3384:0x2b3280)));for(j=0;j<16;++j){gameTypeInfo*row=join?uiInfo.joinGameTypes+j:uiInfo.gameTypes+j;unsigned char*o=b+(join?0x2b3388:0x2b3284)+16*j;const char*names[]={row->gameType,row->gameTypeShort,row->gameTypeDescription};int f,offs[]={0,4,12};assert(row->gtEnum==*(int*)(o+8));for(f=0;f<3;++f){char*s=*(char**)(o+offs[f]);assert((s==NULL)==(names[f]==NULL));if(s)assert(!strcmp(s,names[f]));}}++cases;}}
restore();fprintf(stderr,"hooks restored\n");assert(FreeLibrary(h));fprintf(stderr,"reference DLL unloaded\n");printf("%d original gametype controller/parser comparisons passed\n",cases);return 0;}

