#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_main.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
static char trace[8192],data[32768];static int length,handle;
static int opened(const char*n,fileHandle_t*f,fsMode_t mode){sprintf(trace+strlen(trace),"O:%s:%d;",n,mode);*f=handle;return length;}
static void readFile(void*p,int n,fileHandle_t f){sprintf(trace+strlen(trace),"R:%d:%d;",n,f);assert(n>=0&&n<32768);memcpy(p,data,n);}
static void closed(fileHandle_t f){sprintf(trace+strlen(trace),"C:%d;",f);}
static void printed(const char*s){sprintf(trace+strlen(trace),"P:%s;",s);}
static struct{byte*p,saved[5];}hooks[8];static int hc;
static void patch(void*a,void*f){DWORD old,tmp;byte*p=a;assert(hc<8);hooks[hc].p=p;memcpy(hooks[hc++].saved,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old));p[0]=0xe9;*(int*)(p+1)=(byte*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static void restore(void){DWORD old,tmp;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&old));memcpy(hooks[hc].p,hooks[hc].saved,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,old,&tmp);}}
int main(int argc,char**argv){HMODULE h;byte*b;char expected[8192],*op,*sp;int l,f,d,p,i,cases=0;const int lengths[]={0,1,2,31,1024,32766,32767,32768,32769,100000};const char*names[]={"ui/hostgame.menu","","ui/menu with spaces.menu"};char fallback[]="fallback menu";
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;
 patch(b+0x20610,opened);patch(trap_FS_FOpenFile,opened);patch(b+0x20630,readFile);patch(trap_FS_Read,readFile);patch(b+0x20670,closed);patch(trap_FS_FCloseFile,closed);patch(b+0x204a0,printed);patch(trap_Print,printed);
 for(l=0;l<10;++l)for(f=0;f<3;++f)for(d=0;d<2;++d)for(p=0;p<3;++p){length=lengths[l];handle=f?f*17:0;defaultMenu=d?fallback:NULL;*(char**)(b+0x63f34)=defaultMenu;for(i=0;i<32768;++i)data[i]=(char)((i*37+l*13)&255);
 trace[0]=0;op=((char*(__cdecl*)(const char*))(b+0x4e10))(names[p]);strcpy(expected,trace);trace[0]=0;sp=GetMenuBuffer(names[p]);assert(!strcmp(trace,expected));
 if(!handle||length>=32768){assert(op==defaultMenu&&sp==defaultMenu);}else{assert(op&&sp);assert(!memcmp(op,sp,length+1));assert(!memcmp(sp,data,length)&&sp[length]==0);}++cases;}
 restore();assert(FreeLibrary(h));printf("%d original menu-buffer comparisons passed\n",cases);return 0;}
