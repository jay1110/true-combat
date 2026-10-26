#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_shared.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
static char trace[8192];static int filePresent,writeFailure;static const char*pid,*filePid,*profile;
static int opened(const char*n,fileHandle_t*f,fsMode_t m){sprintf(trace+strlen(trace),"O:%s:%d;",n,m);*f=m==FS_WRITE&&writeFailure==2?-1:29;return m==FS_WRITE?(writeFailure==1?-1:0):(filePresent?31:-1);}
static void readFile(void*p,int n,fileHandle_t f){assert(n==31);sprintf(trace+strlen(trace),"R:%d:%d;",n,f);memset(p,0,n);strncpy(p,filePid,n-1);}
static void writeFile(const void*p,int n,fileHandle_t f){sprintf(trace+strlen(trace),"W:%d:%d:%.*s;",n,f,n,(char*)p);}
static void closed(fileHandle_t f){sprintf(trace+strlen(trace),"C:%d;",f);}
static int deleted(const char*n){sprintf(trace+strlen(trace),"D:%s;",n);return 1;}
static void cvar(const char*n,char*b,int z){sprintf(trace+strlen(trace),"V:%s:%d;",n,z);Q_strncpyz(b,!strcmp(n,"com_pid")?pid:profile,z);}
static void exec(int w,const char*s){sprintf(trace+strlen(trace),"E%d:%s;",w,s);}
static void print(const char*f,...){va_list a;va_start(a,f);vsprintf(trace+strlen(trace),f,a);va_end(a);}
static struct{byte*p,saved[5];}hooks[12];static int hc;
static void patch(void*a,void*f){DWORD o,t;byte*p=a;assert(hc<12);hooks[hc].p=p;memcpy(hooks[hc++].saved,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(byte*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].saved,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}
int main(int argc,char**argv){HMODULE h;byte*b;static displayContextDef_t dc;static byte odc[0x20100];const char*pids[]={"","0","173","-9","173suffix"};const char*profiles[]={"","Player One"};const char*args[]={"","0","1","-1","bad","1 tail"};int mode,p,f,pr,a,cases=0,er,sr;qboolean ea,sa;char expected[8192],ob[128],sb[128],*op,*sp;
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;dc.getCVarString=cvar;dc.executeText=exec;Init_Display(&dc);*(void**)(odc+0x70)=cvar;*(void**)(odc+0xcc)=exec;*(void**)(b+0x245fa4)=odc;
#define PA(off,real,mock) patch(b+(off),mock);patch(real,mock)
 PA(0x20610,trap_FS_FOpenFile,opened);PA(0x20630,trap_FS_Read,readFile);PA(0x20650,trap_FS_Write,writeFile);PA(0x20670,trap_FS_FCloseFile,closed);PA(0x206b0,trap_FS_Delete,deleted);PA(0x2380,Com_Printf,print);
 for(mode=0;mode<3;++mode)for(p=0;p<5;++p)for(f=0;f<5;++f)for(pr=0;pr<2;++pr)for(filePresent=0;filePresent<2;++filePresent)for(writeFailure=0;writeFailure<3;++writeFailure)for(a=0;a<(mode==2?6:1);++a){pid=pids[p];filePid=pids[f];profile=profiles[pr];strcpy(ob,args[a]);strcpy(sb,args[a]);op=ob;sp=sb;ea=sa=qfalse;trace[0]=0;er=sr=0;
  if(mode==0)er=((int(__cdecl*)(char*))(b+0x174b0))("profiles/test/profile.pid");else if(mode==1)er=((int(__cdecl*)(char*))(b+0x17550))("profiles/test/profile.pid");else((void(__cdecl*)(itemDef_t*,qboolean*,char**))(b+0x17600))(NULL,&ea,&op);strcpy(expected,trace);trace[0]=0;
  if(mode==0)sr=Script_CheckProfile("profiles/test/profile.pid");else if(mode==1)sr=Script_WriteProfile("profiles/test/profile.pid");else Script_ExecWolfConfig(NULL,&sa,&sp);
  assert(er==sr&&ea==sa&&!strcmp(trace,expected)&&(op==NULL)==(sp==NULL));if(op)assert(op-ob==sp-sb);++cases;
 }
 restore();assert(FreeLibrary(h));printf("%d original profile/config-chain comparisons passed\n",cases);return 0;}
