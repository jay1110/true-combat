#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_local.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
extern itemDef_t *g_editItem;
extern qboolean g_editingField;
extern int modalMenuCount;
extern menuDef_t *modalMenuStack[];
extern itemDef_t *Menu_ClearFocus(menuDef_t *);
extern void Menu_RunCloseScript(menuDef_t *);
extern void Item_RunScript(itemDef_t *,qboolean *,const char *);
static unsigned char *b,rm[0x800],rp[0x800],ri[4][0x400];static menuDef_t menu,parent;static itemDef_t items[4];
static int original;static char events[4096];
static void patch(void *at,void *fn){DWORD o,t;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void warning(const char *s,...){va_list ap;va_start(ap,s);vsprintf(events+strlen(events),s,ap);va_end(ap);}
static menuDef_t *find(const char *s){sprintf(events+strlen(events),"find:%s;",s);return !strcmp(s,"missing")?NULL:original?(void *)rm:&menu;}
static itemDef_t *clear(menuDef_t *m){strcat(events,"clear;");return NULL;}
static menuDef_t *activate(const char *s,qboolean push){sprintf(events+strlen(events),"activate:%s:%d;",s,push);return NULL;}
static void script(itemDef_t *item,qboolean *abort,const char *s){void *p=original?*(void **)((char *)item+0xec):item->parent;if(p!=(original?(void *)rm:(void *)&menu)||abort)exit(3);sprintf(events+strlen(events),"script:%s;",s);if(original)*(int *)(rm+0x48)^=8;else menu.window.flags^=8;}
int main(int argc,char **argv){HMODULE h;int mode,flags,n,edit,stack,hasScript,missing,hasParent,i,cases=0;char expected[4096];
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
 patch(b+0x15d10,find);patch(Menus_FindByName,find);patch(b+0x15520,clear);patch(Menu_ClearFocus,clear);patch(b+0x17810,script);patch(Item_RunScript,script);patch(b+0x1d9e0,activate);patch(Menus_ActivateByName,activate);patch(b+0x2380,warning);patch(Com_Printf,warning);
 for(mode=0;mode<2;++mode)for(flags=0;flags<16;++flags)for(n=0;n<5;++n)for(edit=0;edit<5;++edit)for(stack=0;stack<3;++stack)for(hasScript=0;hasScript<2;++hasScript)for(missing=0;missing<2;++missing)for(hasParent=0;hasParent<2;++hasParent){
  memset(&menu,0,sizeof(menu));memset(rm,0,sizeof(rm));menu.itemCount=*(int *)(rm+0xc0)=n;menu.cursorItem=*(int *)(rm+0xc8)=3;menu.window.flags=*(int *)(rm+0x48)=(flags&7)|(flags&8?WINDOW_MODAL:0);
  menu.onClose=*(const char **)(rm+0xdc)=hasScript?"close-script":NULL;g_editItem=edit<4?items+edit:NULL;*(void **)(b+0x245fb4)=edit<4?ri[edit]:NULL;g_editingField=*(int *)(b+0x245fac)=1;
  for(i=0;i<n;++i){menu.items[i]=items+i;*(void **)(rm+0x514+4*i)=ri[i];}
  parent.window.name=*(const char **)(rp+0x20)="parent";modalMenuCount=*(int *)(b+0x245fbc)=stack;for(i=0;i<3;++i){modalMenuStack[i]=hasParent?&parent:NULL;*(void **)(b+0x255d60+4*i)=hasParent?rp:NULL;}
  original=1;events[0]=0;if(mode)((void(__cdecl *)(const char *))(b+0x15d80))(missing?"missing":"menu");else ((void(__cdecl *)(void *))(b+0x15e40))(missing?NULL:rm);strcpy(expected,events);
  original=0;events[0]=0;if(mode)Menus_CloseByName(missing?"missing":"menu");else Menu_RunCloseScript(missing?NULL:&menu);
  if(strcmp(events,expected)||menu.window.flags!=*(int *)(rm+0x48)||menu.cursorItem!=*(int *)(rm+0xc8)||modalMenuCount!=*(int *)(b+0x245fbc)||g_editingField!=*(int *)(b+0x245fac)||(g_editItem==NULL)!=(*(void **)(b+0x245fb4)==NULL)){printf("close mismatch mode%d flags%d items%d edit%d stack%d script%d missing%d parent%d\n%s\n%s\n",mode,flags,n,edit,stack,hasScript,missing,hasParent,expected,events);return 1;}++cases;
 }
 printf("%d whole Windows menu-close/close-script comparisons passed\n",cases);return 0;
}
