#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_local.h"
#include <windows.h>
#include <stdio.h>
extern displayContextDef_t *DC;
extern void Script_SetPlayerModel(itemDef_t *,qboolean *,char **);
extern void Script_SetPlayerHead(itemDef_t *,qboolean *,char **);
extern void Script_SetFocus(itemDef_t *,qboolean *,char **);
extern void Script_ClearFocus(itemDef_t *,qboolean *,char **);
extern itemDef_t *Menu_ClearFocus(menuDef_t *);
extern void Item_RunScript(itemDef_t *,qboolean *,const char *);
extern void Item_MouseLeave(itemDef_t *);
static char events[2048];static int original;
static void patch(void *a,void *f){DWORD old,tmp;unsigned char *p=a;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static const char *name(void *p){return original?*(const char **)((char *)p+0x20):((itemDef_t *)p)->window.name;}
static void script(void *p,void *abort,const char *s){strcat(events,va("script:%s:%s;",name(p),s));}
static void leave(void *p){strcat(events,va("leave:%s;",name(p)));}
static void setCvar(const char *k,const char *v){strcat(events,va("set:%s:%s;",k,v));}
static void sound(int s,int ch){strcat(events,va("sound:%d:%d;",s,ch));}
int main(int argc,char **argv){HMODULE h;unsigned char *b;static unsigned char rm[0x718],ri[3][0x400],rdc[0x1e400];static menuDef_t menu;static itemDef_t items[3];static displayContextDef_t dc;const char *names[]={"first","second","third"};const char *targets[]={"first ;","SECOND ;","third ;","absent ;",""};int seed,i,t,s,mode,count=0;char a[128],c[128],*ap,*cp,expected[2048];void *r1;itemDef_t *r2;qboolean abort1,abort2;
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
 DC=&dc;*(void **)(b+0x245fa4)=rdc;dc.startLocalSound=sound;*(void **)(rdc+0x88)=sound;
 patch(b+0x17810,script);patch(Item_RunScript,script);patch(b+0x185c0,leave);patch(Item_MouseLeave,leave);
 dc.setCVar=setCvar;*(void **)(rdc+0x78)=setCvar;
 {const char *values[]={"soldier ;","\"temperate/allied/elite\" ;","\"head with spaces\" ;","\"\" ;","",";"};
 for(mode=0;mode<2;++mode)for(t=0;t<6;++t){
  strcpy(a,values[t]);strcpy(c,a);ap=a;cp=c;abort1=abort2=qtrue;
  events[0]=0;((void(__cdecl *)(void *,void *,char **))(b+(mode?0x170c0:0x17080)))(NULL,&abort1,&ap);strcpy(expected,events);
  events[0]=0;if(mode)Script_SetPlayerHead(NULL,&abort2,&cp);else Script_SetPlayerModel(NULL,&abort2,&cp);
  if(strcmp(expected,events)||abort1!=abort2||(ap==NULL)!=(cp==NULL)||(ap&&cp&&ap-a!=cp-c))return 4;
 }}
 if(Menu_ClearFocus(NULL)!=NULL || ((void *(__cdecl *)(void *))(b+0x15520))(NULL)!=NULL)return 3;
 for(mode=0;mode<3;++mode)for(seed=0;seed<64;++seed)for(t=0;t<5;++t)for(s=0;s<2;++s){
 memset(&menu,0,sizeof(menu));memset(rm,0,sizeof(rm));menu.itemCount=*(int *)(rm+0xc0)=3;
 dc.Assets.itemFocusSound=*(int *)(rdc+0x1e324)=s?123:0;
 for(i=0;i<3;++i){memset(&items[i],0,sizeof(items[i]));memset(ri[i],0,sizeof(ri[i]));items[i].window.name=*(const char **)(ri[i]+0x20)=names[i];items[i].parent=&menu;*(void **)(ri[i]+0xec)=rm;
 items[i].window.flags=*(int *)(ri[i]+0x48)=((seed>>(i*2))&3)|((seed&32)&&i==2?WINDOW_DECORATION:0);
 items[i].onFocus=*(const char **)(ri[i]+0x10c)=seed&1?"focus-script":NULL;items[i].leaveFocus=*(const char **)(ri[i]+0x110)=seed&2?"leave-script":NULL;
 menu.items[i]=items+i;*(void **)(rm+0x514+i*4)=ri[i];}
 strcpy(a,targets[t]);strcpy(c,a);ap=a;cp=c;abort1=abort2=qtrue;r1=NULL;r2=NULL;
 original=1;events[0]=0;if(mode==0)((void(__cdecl *)(void *,void *,char **))(b+0x16fc0))(ri[0],&abort1,&ap);else if(mode==1)((void(__cdecl *)(void *,void *,char **))(b+0x17060))(ri[0],&abort1,&ap);else r1=((void *(__cdecl *)(void *))(b+0x15520))(rm);strcpy(expected,events);
 original=0;events[0]=0;if(mode==0)Script_SetFocus(items,&abort2,&cp);else if(mode==1)Script_ClearFocus(items,&abort2,&cp);else r2=Menu_ClearFocus(&menu);
 if(strcmp(expected,events)||abort1!=abort2||(ap==NULL)!=(cp==NULL)||(ap&&cp&&ap-a!=cp-c)){printf("focus event mismatch mode%d seed%d target%s\n%s\n%s\n",mode,seed,a,expected,events);return 1;}
 for(i=0;i<3;++i)if(items[i].window.flags!=*(int *)(ri[i]+0x48)||(r1==ri[i])!=(r2==items+i)){printf("focus state mismatch mode%d seed%d item%d\n",mode,seed,i);return 1;}++count;
 }
 printf("%d whole SetFocus/ClearFocus/Menu_ClearFocus comparisons plus12 player-model/head callback cases passed\n",count);return 0;
}
