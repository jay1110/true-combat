#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_shared.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
static itemDef_t items[8];static menuDef_t menu;static displayContextDef_t dc;static byte rawDC[0x20100];
static int mask,trace[32],calls,moves,parseOK,parses;static float mx,my;static const char*script="setcvar ui_probe 1 ;";
static qboolean focus(itemDef_t*i,float x,float y){int idx=(int)(i-items);assert(idx>=0&&idx<8&&x==dc.cursorx&&y==dc.cursory);assert(calls<32);trace[calls++]=idx;return (mask&(1<<idx))?qtrue:qfalse;}
static void move(menuDef_t*m,float x,float y){assert(m==&menu);++moves;mx=x;my=y;}
static qboolean parse(int h,const char**p){assert(h==71);++parses;if(!parseOK)return qfalse;*p=script;return qtrue;}
static struct{byte*p,saved[5];}hooks[6];static int hc;
static void patch(void*a,void*f){DWORD o,t;byte*p=a;hooks[hc].p=p;memcpy(hooks[hc++].saved,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(byte*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].saved,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}
int main(int argc,char**argv){HMODULE h;byte*b;menuDef_t before,after;itemDef_t*er,*sr;itemDef_t input,out;int mode,n,cur,i,pos,ec,em,et[32],cases=0,parserCases=0;float ex,ey;void*oldDC;
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;assert(sizeof(menuDef_t)==0x718&&sizeof(itemDef_t)==0x274);DC=&dc;oldDC=*(void**)(b+0x245fa4);*(void**)(b+0x245fa4)=rawDC;
 patch(b+0x17b30,focus);patch(Item_SetFocus,focus);patch(b+0x1dae0,move);patch(Menu_HandleMouseMove,move);patch(b+0x14d40,parse);patch(PC_Script_Parse,parse);
 for(mode=0;mode<2;++mode)for(n=0;n<=6;++n)for(cur=-1;cur<=n;++cur)for(mask=0;mask<(1<<n);++mask)for(pos=0;pos<3;++pos){
  /* next(-1) can return items[-1] when no later item accepts focus: undefined C excluded. */
  if(mode&&cur==-1&&n>0&&!(mask&~1))continue;
  memset(&menu,0,sizeof(menu));menu.itemCount=n;menu.cursorItem=cur;for(i=0;i<8;++i){memset(items+i,0,sizeof(items[i]));items[i].window.rect.x=(float)(i*17.31-53.273+pos*.113);items[i].window.rect.y=(float)(i*.713+17.34-pos*.371);menu.items[i]=items+i;}
  dc.cursorx=pos*123-91;dc.cursory=pos*97-53;*(int*)(rawDC+0x128)=dc.cursorx;*(int*)(rawDC+0x12c)=dc.cursory;before=menu;
  calls=moves=0;mx=my=0;er=((itemDef_t*(__cdecl*)(menuDef_t*))(b+(mode?0x19e80:0x19da0)))(&menu);after=menu;ec=calls;em=moves;ex=mx;ey=my;memcpy(et,trace,sizeof(et));
  menu=before;calls=moves=0;mx=my=0;sr=mode?Menu_SetNextCursorItem(&menu):Menu_SetPrevCursorItem(&menu);
  assert(er==sr&&!memcmp(&menu,&after,sizeof(menu)));assert(calls==ec&&moves==em&&mx==ex&&my==ey&&!memcmp(et,trace,calls*sizeof(int)));++cases;
 }
 assert(((itemDef_t*(__cdecl*)(menuDef_t*))(b+0x19e80))(NULL)==Menu_SetNextCursorItem(NULL));++cases;
 for(mode=0;mode<2;++mode)for(parseOK=0;parseOK<2;++parseOK)for(i=0;i<16;++i){int a,c;memset(&input,i*13,sizeof(input));out=input;parses=0;a=((int(__cdecl*)(itemDef_t*,int))(b+(mode?0x1ea30:0x1ea10)))(&out,71);assert(parses==1);parses=0;c=mode?ItemParse_leaveFocus(&input,71):ItemParse_onFocus(&input,71);assert(a==c&&parses==1&&!memcmp(&input,&out,sizeof(input)));++parserCases;}
 *(void**)(b+0x245fa4)=oldDC;restore();assert(FreeLibrary(h));printf("%d original cursor controller and %d focus parser comparisons passed\n",cases,parserCases);return 0;}
