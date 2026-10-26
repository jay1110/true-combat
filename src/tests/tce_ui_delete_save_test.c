#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_main.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
static char trace[2048];static int first,second,calls;
static char fileList[4096],loadTrace[32768];static int fileCount;
static int files(const char*p,const char*e,char*out,int size){assert(!strcmp(p,"save")&&!strcmp(e,"svg")&&size==4096);memcpy(out,fileList,4096);return fileCount;}
static qhandle_t shader(const char*n){sprintf(loadTrace+strlen(loadTrace),"%s;",n);return (qhandle_t)strlen(n);}
static int del(const char*n){sprintf(trace+strlen(trace),"D[%s];",n);return calls++?second:first;}
static void print(const char*n,...){va_list a;va_start(a,n);vsprintf(trace+strlen(trace),n,a);va_end(a);}
static void patch(void*at,void*fn){unsigned char*p=at;DWORD o,t;assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
int main(int argc,char**argv){HMODULE h;unsigned char*b;int i,a,c,cases=0;char expected[2048];const char*names[]={"QUICKSAVE","Mixed Case","","slot.svg","name%percent"};
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;patch(b+0x206b0,del);patch(trap_FS_Delete,del);patch(b+0x2380,print);patch(Com_Printf,print);
 for(i=0;i<5;++i)for(a=-1;a<=1;++a)for(c=-1;c<=1;++c){first=a;second=c;uiInfo.savegameIndex=*(int*)(b+0x2cf2ec)=i;uiInfo.savegameList[i].name=*(const char**)(b+0x2ceae8+i*8)=names[i];
  trace[0]=0;calls=0;((void(__cdecl*)(void))(b+0xafe0))();assert(calls==2);strcpy(expected,trace);
  trace[0]=0;calls=0;UI_DelSavegame();assert(calls==2&&!strcmp(expected,trace));++cases;}
 printf("%d original save deletion callback comparisons passed (no files deleted)\n",cases);
 patch(b+0x20690,files);patch(trap_FS_GetFileList,files);patch(b+0x20730,shader);patch(trap_R_RegisterShaderNoMip,shader);
 {int scenario,j,pos,expectedCount,loads=0;char expectedTrace[32768],expectedNames[256][64];int expectedShaders[256];
  for(scenario=0;scenario<6;++scenario){memset(fileList,0,sizeof(fileList));pos=0;
   fileCount=scenario==0?0:scenario==1?1:scenario==2?6:scenario==3?256:scenario==4?257:12;
   for(j=0;j<fileCount;++j){char name[48];
    if(scenario==1||scenario==5)sprintf(name,"current%d.svg",j);
    else if(scenario==2){const char*variants[]={"current.svg","Current.svg","lower.svg","MIXED.SvG","keep.ext","current_extra.svg"};strcpy(name,variants[j]);}
    else sprintf(name,"s%03d.svg",j);
    assert(pos+(int)strlen(name)+1<4096);strcpy(fileList+pos,name);pos+=(int)strlen(name)+1;
   }
   /* Real String_Init resets each independent allocator; no allocation stub. */
   ((void(__cdecl*)(void))(b+0x14af0))();String_Init();
   loadTrace[0]=0;((void(__cdecl*)(void))(b+0xb060))();strcpy(expectedTrace,loadTrace);expectedCount=*(int*)(b+0x2cf2e8);
   assert(expectedCount>=0&&expectedCount<=256);
   for(j=0;j<expectedCount;++j){strcpy(expectedNames[j],*(const char**)(b+0x2ceae8+j*8));expectedShaders[j]=*(int*)(b+0x2ceaec+j*8);}
   loadTrace[0]=0;UI_LoadSavegames();assert(uiInfo.savegameCount==expectedCount&&!strcmp(loadTrace,expectedTrace));
   for(j=0;j<expectedCount;++j){assert(!strcmp(uiInfo.savegameList[j].name,expectedNames[j]));assert(uiInfo.savegameList[j].sshotImage==expectedShaders[j]);}
   ++loads;
  }
  printf("%d whole original save-list cases passed, including 256/257 limits (no file access)\n",loads);
 }
 FreeLibrary(h);return 0;}
