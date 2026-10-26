#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
static char message[1024];static int recipient,count;static byte rawClient[0x16b0];static gclient_t client;
static void capture(int n,const char*s){recipient=n;count++;strcpy(message,s);}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;int(__cdecl*original)(void*,const char*);int k;static const char*commands[]={"pause","ready","unpause","","^3test"};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x55960);hook(b+0x8ad60,capture);hook(trap_SendServerCommand,capture);
 for(k=0;k<20000;++k){int n=k%64,delta=k%10001-5000,x,y,ec,er;char expected[1024];byte*e=b+0x226e2a0+n*0x57c;g_entities[n].client=&client;*(void**)(e+0x190)=rawClient;level.time=*(int*)(b+0x7febe0)=100000+k;client.pers.cmd_debounce=*(int*)(rawClient+0xa20)=level.time+delta;
  count=0;message[0]=0;recipient=-1;x=original(e,commands[k%5]);ec=count;er=recipient;strcpy(expected,message);count=0;message[0]=0;recipient=-1;y=G_cmdDebounce(&g_entities[n],commands[k%5]);
  if(x!=y||client.pers.cmd_debounce!=*(int*)(rawClient+0xa20)||count!=ec||recipient!=er||strcmp(message,expected)){printf("debounce case%d mismatch ret%d/%d count%d/%d recipient%d/%d source[%s] original[%s]\n",k,y,x,count,ec,recipient,er,message,expected);return 1;}
 }
 puts("20000 whole G_cmdDebounce original comparisons passed; exact messages/deadlines/recipients");return 0;
}

