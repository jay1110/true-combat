#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_shared.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stddef.h>
static char trace[8192];
static void stopped(int h){sprintf(trace+strlen(trace),"C%d;",h);}
static void executed(int when,const char*s){sprintf(trace+strlen(trace),"E%d:%s;",when,s);}
int main(int argc,char**argv){
 HMODULE h;unsigned char*b;static displayContextDef_t dc;static byte odc[0x20100];
 menuDef_t sm,om;itemDef_t si[4];byte oi[4][0x268];char expected[8192];
 const char*names[]={"map_one","map_two","other","MAP_ONE"};
 const char*groups[]={"maps","maps",NULL,"other"};
 const char*queries[]={"map_one","MAP_ONE","maps","MAPS","map_*","MAP_*","*","absent","other"};
 int mode,n,q,f,v,c,i,cases=0;
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;
 assert(sizeof(menuDef_t)==0x718);assert(offsetof(menuDef_t,items)==0x514);
 assert(offsetof(itemDef_t,window.flags)==0x48);assert(offsetof(itemDef_t,window.cinematic)==0x30);
 dc.stopCinematic=stopped;dc.executeText=executed;Init_Display(&dc);
 *(void**)(odc+0xf0)=stopped;*(void**)(odc+0xcc)=executed;*(void**)(b+0x245fa4)=odc;
 String_Init();((void(__cdecl*)(void))(b+0x14af0))();
 for(mode=0;mode<2;++mode)for(n=0;n<=4;++n)for(q=0;q<9;++q)for(f=0;f<32;++f)for(v=0;v<2;++v)for(c=0;c<3;++c){
  memset(&sm,0,sizeof(sm));memset(&om,0,sizeof(om));memset(si,0,sizeof(si));memset(oi,0,sizeof(oi));sm.itemCount=om.itemCount=n;
  for(i=0;i<4;++i){
   si[i].window.name=names[i];si[i].window.group=groups[i];
   si[i].window.flags=0x20000000|((f&1)?WINDOW_VISIBLE:0)|((f&2)?WINDOW_MOUSEOVER:0)|((f&4)?WINDOW_MOUSEOVERTEXT:0)|((f&8)?WINDOW_FADINGIN:0)|((f&16)?WINDOW_FADINGOUT:0)|WINDOW_LB_LEFTARROW|WINDOW_LB_RIGHTARROW;
   si[i].window.cinematic=c==0?-2:c==1?-1:i+17;
   si[i].mouseExitText="exec \"text_exit\"";si[i].mouseExit="exec \"item_exit\"";
   memcpy(oi[i],&si[i].window,sizeof(si[i].window));*(const char**)(oi[i]+0xf8)=si[i].mouseExitText;*(const char**)(oi[i]+0x100)=si[i].mouseExit;
   sm.items[i]=si+i;om.items[i]=(itemDef_t*)oi[i];
  }
  trace[0]=0;((void(__cdecl*)(menuDef_t*,const char*,int))(b+(mode?0x15cb0:0x15c20)))(&om,queries[q],v);strcpy(expected,trace);
  trace[0]=0;if(mode)Menu_FadeItemByName(&sm,queries[q],v);else Menu_ShowItemByName(&sm,queries[q],v);
  assert(!strcmp(trace,expected));for(i=0;i<4;++i)assert(!memcmp(&si[i].window,oi[i],sizeof(si[i].window)));++cases;
 }
 assert(FreeLibrary(h));printf("%d original menu visibility/fade comparisons passed\n",cases);return 0;
}
