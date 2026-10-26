#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
static char stream[16][1024],events[8192];static int nt,pos,mode,loads;
static int load(const char*s){strcat(events,s);strcat(events,"\n");loads++;return mode==0?0:mode==1&&loads==1?0:17;}
static int readToken(int h,pc_token_t*t){char tmp[32];sprintf(tmp,"read:%d\n",h);strcat(events,tmp);if(pos==nt)return 0;memset(t,0,sizeof(*t));strcpy(t->string,stream[pos++]);t->type=TT_NUMBER;t->intvalue=atoi(t->string);return 1;}
static void release(int h){char tmp[32];sprintf(tmp,"free:%d\n",h);strcat(events,tmp);}
static void print(const char*s){strcat(events,s);}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;void(__cdecl*original)(void);int c,i,n=0;static char expected[8192];int gt[]={2,5,7};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x47d70);
 hook(b+0x65780,load);hook(trap_PC_LoadSource,load);hook(b+0x657c0,readToken);hook(trap_PC_ReadToken,readToken);hook(b+0x657a0,release);hook(trap_PC_FreeSource,release);hook(b+0x64b80,print);hook(trap_Print,print);
 for(c=0;c<3600;c++){char number[20];mode=c%3;cg_gameType.integer=*(int*)(b+0x488c18c)=gt[(c/3)%3];strcpy(cgs.rawmapname,"OBJ_NoRthPort");strcpy((char*)b+0x257e150,cgs.rawmapname);
  memset(cg.objMapDescription_Axis,0x55,384*3+256*16);memset(b+0x40a28b0,0x55,384*3+256*16);
  strcpy(stream[0],"wm_mapdescription");strcpy(stream[1],c%2?"axis":"allied");strcpy(stream[2],"Objective map briefing");strcpy(stream[3],"wm_objective_axis_desc");sprintf(number,"%d",(c/9)%10);strcpy(stream[4],number);strcpy(stream[5],"Plant at the marked objective");strcpy(stream[6],"wm_objective_allied_desc");strcpy(stream[7],number);strcpy(stream[8],"Defend the marked objective");
  for(i=0;i<(c%500);i++)stream[5][i]='a'+i%26;stream[5][c%500]=0;nt=(c/90)%10;pos=loads=0;events[0]=0;original();strcpy(expected,events);pos=loads=0;events[0]=0;CG_LoadObjectiveData();
  if(strcmp(events,expected)||strcmp(cgs.rawmapname,(char*)b+0x257e150)||memcmp(cg.objMapDescription_Axis,b+0x40a28b0,384*3+256*16)){printf("objective data case%d mode%d gt%d tokens%d\n",c,mode,cg_gameType.integer,nt);return 1;}n++;
 }
 printf("%d whole GT2/5/7 objective loader comparisons passed\n",n);return 0;
}
