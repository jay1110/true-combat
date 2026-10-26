#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_local.h"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
extern menuDef_t Menus[],*modalMenuStack[];extern int menuCount,modalMenuCount;
extern void Menus_Activate(menuDef_t *);
static unsigned char *b;static int original,mutation;static jmp_buf escape;static char events[1024];
#ifdef CGAMEDLL
#define TABLE 0x1819560
#define COUNT 0x1819170
#define MODALS 0x1819174
#define STACK 0x1819500
#define FOCUS 0x85a10
#define ACTIVATE 0x85a60
#define OPEN 0x82bc0
#define ERRORFN 0x47c30
#else
#define TABLE 0x255da0
#define COUNT 0x245fb8
#define MODALS 0x245fbc
#define STACK 0x255d60
#define FOCUS 0x1d8a0
#define ACTIVATE 0x1d9e0
#define OPEN 0x19f80
#define ERRORFN 0x2340
#endif
static void patch(void *at,void *fn){DWORD o,t;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static int indexof(void *p,int orig){return !p?-1:orig?((unsigned char *)p-b-TABLE)/0x718:(menuDef_t *)p-Menus;}
static void openmenu(menuDef_t *p){int i=indexof(p,original);sprintf(events+strlen(events),"open:%d;",i);if(original)*(int *)((char *)p+0x48)^=mutation;else p->window.flags^=mutation;}
static void err(int code,const char *fmt,...){sprintf(events+strlen(events),"error:%d:%s;",code,fmt);longjmp(escape,1);}
int main(int argc,char **argv){HMODULE h;int n,f,q,m,c,u,i,cases=0,expectedIndex,actualIndex,failed;char expected[1024];const char *names[]={"first","SECOND","first",""},*queries[]={"first","second","missing","",NULL};int counts[]={0,1,4,16},modalCounts[]={0,15,16};
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;patch(b+OPEN,openmenu);patch(Menus_Activate,openmenu);patch(b+ERRORFN,err);patch(Com_Error,err);
 for(n=0;n<4;++n)for(f=0;f<8;++f)for(q=0;q<5;++q)for(m=0;m<2;++m)for(c=0;c<3;++c)for(u=0;u<2;++u){
  menuCount=*(int *)(b+COUNT)=counts[n];modalMenuCount=*(int *)(b+MODALS)=modalCounts[c];mutation=u?WINDOW_MODAL:0;
  memset(modalMenuStack,0,16*sizeof(void *));memset(b+STACK,0,16*4);
  for(i=0;i<menuCount;++i){unsigned char *r=b+TABLE+i*0x718;memset(Menus+i,0,sizeof(menuDef_t));memset(r,0,0x718);Menus[i].window.name=*(const char **)(r+0x20)=names[i%4];Menus[i].window.flags=*(int *)(r+0x48)=(f+i)%8|(i%2?WINDOW_MODAL:0);}
  if(indexof(((void *(__cdecl *)(void))(b+FOCUS))(),1)!=indexof(Menu_GetFocused(),0))return 3;
  original=1;events[0]=0;expectedIndex=-2;failed=setjmp(escape);if(!failed)expectedIndex=indexof(((void *(__cdecl *)(const char *,int))(b+ACTIVATE))(queries[q],m),1);strcpy(expected,events);
  original=0;events[0]=0;actualIndex=-2;if(!setjmp(escape))actualIndex=indexof(Menus_ActivateByName(queries[q],m),0);
  if(expectedIndex!=actualIndex||strcmp(expected,events)||modalMenuCount!=*(int *)(b+MODALS)){printf("case%d mismatch %s / %s\n",cases,expected,events);return 1;}
  for(i=0;i<menuCount;++i)if(Menus[i].window.flags!=*(int *)(b+TABLE+i*0x718+0x48))return 4;
  for(i=0;i<16;++i)if(indexof(modalMenuStack[i],0)!=indexof(*(void **)(b+STACK+i*4),1))return 5;
  ++cases;
 }
 printf("%d Windows menu focus/activation comparisons passed\n",cases);return 0;
}
