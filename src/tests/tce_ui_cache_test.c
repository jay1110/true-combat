#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_shared.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>

static char trace[8192];
static int resultHandle;
static int play(const char *s,float x,float y,float w,float h) {
 sprintf(trace+strlen(trace),"P[%s]:%g,%g,%g,%g;",s,x,y,w,h);return resultHandle;
}
static void stop(int h){sprintf(trace+strlen(trace),"X%d;",h);}
static sfxHandle_t sound(const char *s,qboolean c){sprintf(trace+strlen(trace),"S[%s]:%d;",s,c);return 99;}
int main(int argc,char **argv) {
 HMODULE module;unsigned char *b,*m;displayContextDef_t dc;
 unsigned char rawItems[4][0x268];itemDef_t items[4];
 const char *names[]={NULL,"","movie/intro"};
 int n,a,s,h,i,t,cases=0;char expected[8192];
 assert(argc==2);module=LoadLibraryA(argv[1]);assert(module);b=(unsigned char*)module;
 memset(&dc,0,sizeof(dc));dc.playCinematic=play;dc.stopCinematic=stop;dc.registerSound=sound;
 /* Display-context prefix is shared ABI; explicitly guard callback offsets. */
 assert((char*)&dc.registerSound-(char*)&dc==0xe0);
 assert((char*)&dc.playCinematic-(char*)&dc==0xec);
 assert((char*)&dc.stopCinematic-(char*)&dc==0xf0);
 Init_Display(&dc);*(displayContextDef_t**)(b+0x245fa4)=&dc;
 for(n=0;n<=4;++n)for(a=0;a<3;++a)for(s=0;s<3;++s)for(h=0;h<3;++h){
  resultHandle=h==0?-1:h==1?0:23;
  memset(Menus,0,2*sizeof(menuDef_t));memset(b+0x255da0,0,2*0x718);
  memset(items,0,sizeof(items));memset(rawItems,0,sizeof(rawItems));
  for(i=0;i<4;++i){items[i].window.cinematicName=names[(a+i)%3];*(const char**)(rawItems[i]+0x2c)=items[i].window.cinematicName;}
  for(t=0;t<2;++t){m=b+0x255da0+t*0x718;Menus[t].window.cinematicName=names[a];*(const char**)(m+0x2c)=names[a];
   Menus[t].soundName=names[s];*(const char**)(m+0x4f0)=names[s];Menus[t].itemCount=*(int*)(m+0xc0)=n;
   for(i=0;i<n;++i){Menus[t].items[i]=i==2?NULL:&items[i];*(void**)(m+0x514+4*i)=i==2?NULL:rawItems[i];}
  }
  for(t=0;t<4;++t){trace[0]=0;
   if(t==0)((void(__cdecl*)(void*))(b+0x1fec0))(rawItems[0]);
   if(t==1)((void(__cdecl*)(void*))(b+0x1ff00))(rawItems[0]);
   if(t==2)((void(__cdecl*)(void*))(b+0x1fe50))(b+0x255da0);
   if(t==3){menuCount=*(int*)(b+0x245fb8)=n%3;((void(__cdecl*)(void))(b+0x1fe20))();}
   strcpy(expected,trace);trace[0]=0;
   if(t==0)Window_CacheContents(&items[0].window);
   if(t==1)Item_CacheContents(&items[0]);
   if(t==2)Menu_CacheContents(Menus);
   if(t==3)Display_CacheAll();
   if(strcmp(expected,trace)){printf("case%d target%d: %s / %s\n",cases,t,expected,trace);return 1;}++cases;
  }
 }
 for(t=0;t<3;++t){trace[0]=0;((void(__cdecl*)(void*))(b+(t==0?0x1fec0:t==1?0x1ff00:0x1fe50)))(NULL);
  strcpy(expected,trace);trace[0]=0;if(t==0)Window_CacheContents(NULL);if(t==1)Item_CacheContents(NULL);if(t==2)Menu_CacheContents(NULL);
  assert(!strcmp(expected,trace));++cases;}
 printf("%d whole UI cache original callback-trace comparisons passed\n",cases);FreeLibrary(module);return 0;
}
