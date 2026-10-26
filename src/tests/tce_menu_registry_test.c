#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_local.h"
#include <windows.h>
#include <stdio.h>
extern menuDef_t Menus[];extern int menuCount;
extern void Item_RunScript(itemDef_t *,qboolean *,const char *);
static unsigned char *b;static int original;static char events[8192];
static void patch(void *a,void *f){DWORD o,t;unsigned char *p=a;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void script(itemDef_t *item,qboolean *abort,const char *s){void *parent=original?*(void **)((char *)item+0xec):item->parent;int index=original?((unsigned char *)parent-b-0x255da0)/0x718:(menuDef_t *)parent-Menus;sprintf(events+strlen(events),"script:%d:%s:%d;",index,s,abort!=NULL);if(original)*(int *)((char *)parent+0x48)^=8;else ((menuDef_t *)parent)->window.flags^=8;}
int main(int argc,char **argv){HMODULE h;int n,f,q,i,cases=0,expectedIndex,actualIndex;char expected[8192];const int counts[]={-1,0,1,4,16,64};const char *names[]={"first","SECOND","first",""},*queries[]={"first","second","missing","",NULL};
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;patch(b+0x17810,script);patch(Item_RunScript,script);
 for(n=0;n<6;++n)for(f=0;f<32;++f)for(q=0;q<5;++q){
  menuCount=*(int *)(b+0x245fb8)=counts[n];for(i=0;i<counts[n];++i){unsigned char *r=b+0x255da0+i*0x718;memset(Menus+i,0,sizeof(menuDef_t));memset(r,0,0x718);Menus[i].window.name=*(const char **)(r+0x20)=names[i%4];Menus[i].window.flags=*(int *)(r+0x48)=f;Menus[i].onClose=*(const char **)(r+0xdc)=i%2?NULL:"closed";}
  {void *r=((void *(__cdecl *)(const char *))(b+0x15d10))(queries[q]);menuDef_t *m=Menus_FindByName(queries[q]);expectedIndex=r?((unsigned char *)r-b-0x255da0)/0x718:-1;actualIndex=m?m-Menus:-1;if(expectedIndex!=actualIndex)return 1;}
  original=1;events[0]=0;((void(__cdecl *)(void))(b+0x15e80))();strcpy(expected,events);original=0;events[0]=0;Menus_CloseAll();if(strcmp(events,expected))return 2;
  for(i=0;i<counts[n];++i)if(Menus[i].window.flags!=*(int *)(b+0x255de8+i*0x718))return 3;++cases;
 }
 printf("%d complete Windows menu registry/close-all comparisons passed\n",cases);return 0;
}
