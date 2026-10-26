#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_shared.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
static int calls,lastIndex;static float lastFeeder;
static void selected(float f,int i){++calls;lastFeeder=f;lastIndex=i;}
int main(int argc,char **argv){
 HMODULE lib;unsigned char *b,*m;displayContextDef_t dc;
 unsigned char rawItems[3][0x268],rawLists[3][512];itemDef_t items[3];listBoxDef_t lists[3];
 const char *names[]={"first","second"};int n,k,idx,route,i,j,cases=0,ec,ei;float ef;
 assert(argc==2);lib=LoadLibraryA(argv[1]);assert(lib);b=(unsigned char*)lib;
 memset(&dc,0,sizeof(dc));dc.feederSelection=selected;Init_Display(&dc);*(void**)(b+0x245fa4)=&dc;
 assert(sizeof(menuDef_t)==0x718);
 for(k=0;k<20;++k){menuDef_t out;unsigned char raw[0x718];
  memset(&out,0xab,sizeof(out));memset(raw,0xab,sizeof(raw));
  dc.Assets.fadeAmount=k*.01f;dc.Assets.fadeClamp=k*.05f;dc.Assets.fadeCycle=k*7;
  assert((char*)&dc.Assets.fadeAmount-(char*)&dc==0x1e330);
  ((void(__cdecl*)(void*))(b+0x1d830))(raw);Menu_Init(&out);
  assert(!memcmp(&out,raw,sizeof(out)));++cases;
 }
 for(n=0;n<=3;++n)for(k=0;k<3;++k)for(idx=-1;idx<=2;++idx)for(route=0;route<4;++route){
  memset(Menus,0,2*sizeof(menuDef_t));memset(b+0x255da0,0,2*0x718);
  memset(items,0,sizeof(items));memset(rawItems,0,sizeof(rawItems));memset(lists,0,sizeof(lists));memset(rawLists,0,sizeof(rawLists));
  menuCount=*(int*)(b+0x245fb8)=2;
  for(i=0;i<3;++i){items[i].special=i%2;*(float*)(rawItems[i]+0x248)=i%2;
   items[i].cursorPos=*(int*)(rawItems[i]+0x24c)=17;items[i].typeData=&lists[i];*(void**)(rawItems[i]+0x250)=rawLists[i];
   lists[i].cursorPos=*(int*)(rawLists[i]+12)=11;lists[i].startPos=*(int*)rawLists[i]=9;}
  for(j=0;j<2;++j){m=b+0x255da0+j*0x718;Menus[j].window.name=*(const char**)(m+0x20)=names[j];
   Menus[j].window.flags=*(int*)(m+0x48)=j?WINDOW_VISIBLE|WINDOW_HASFOCUS:0;Menus[j].fullScreen=*(int*)(m+0xbc)=k==j;
   Menus[j].itemCount=*(int*)(m+0xc0)=n;
   for(i=0;i<n;++i){Menus[j].items[i]=&items[i];*(void**)(m+0x514+i*4)=rawItems[i];}}
  assert(Menus_AnyFullScreenVisible()==((int(__cdecl*)(void))(b+0x1d9b0))());
  calls=0;lastIndex=0;lastFeeder=0;
  ((void(__cdecl*)(void*,int,int,const char*))(b+0x1d8f0))(route==0?b+0x255da0:NULL,k,idx,route==2?"second":route==3?"absent":NULL);
  ec=calls;ei=lastIndex;ef=lastFeeder;calls=0;lastIndex=0;lastFeeder=0;
  Menu_SetFeederSelection(route==0?Menus:NULL,k,idx,route==2?"second":route==3?"absent":NULL);
  assert(calls==ec&&lastIndex==ei&&lastFeeder==ef);
  for(i=0;i<3;++i){assert(items[i].cursorPos==*(int*)(rawItems[i]+0x24c));assert(lists[i].startPos==*(int*)rawLists[i]);assert(lists[i].cursorPos==*(int*)(rawLists[i]+12));}++cases;
 }
 for(i=1;i<256;++i){char text[33];for(j=0;j<32;++j)text[j]=(char)(1+(i+j*19)%255);text[32]=0;
  assert(KeywordHash_Key(text)==((int(__cdecl*)(char*))(b+0x1e030))(text));}
 assert(KeywordHash_Key("")==((int(__cdecl*)(char*))(b+0x1e030))("") );
 puts("212 original menu init/selection cases, 192 fullscreen and 256 keyword hash comparisons passed");FreeLibrary(lib);return 0;
}
