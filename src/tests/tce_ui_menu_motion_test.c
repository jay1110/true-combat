#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_shared.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
int main(int argc,char**argv){
 HMODULE h;byte*b;menuDef_t sm,om;itemDef_t si[3];byte oi[3][0x280];
 const char*names[]={"map_one","map_two","other"};const char*groups[]={"maps",NULL,"maps"};
 const char*queries[]={"map_one","MAP_ONE","maps","map_*","*","absent"};
 const float amounts[]={1,2,3.7f,0.3f,-2.3f};
 int mode,n,q,k,a,parent,i,cases=0;rectDef_t from,to;
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;assert(sizeof(menuDef_t)==0x718);
 for(mode=0;mode<2;++mode)for(n=0;n<=3;++n)for(q=0;q<6;++q)for(k=0;k<32;++k)for(a=0;a<5;++a)for(parent=0;parent<2;++parent){
  memset(&sm,0,sizeof(sm));memset(&om,0,sizeof(om));memset(si,0,sizeof(si));memset(oi,0,sizeof(oi));
  sm.itemCount=om.itemCount=n;sm.window.rect.x=om.window.rect.x=k*.13f-37.7f;sm.window.rect.y=om.window.rect.y=k*.73f+12.1f;
  from.x=k*.37f-3.1f;from.y=k*1.71f-81.3f;from.w=k*.13f+4.7f;from.h=k*.93f+9.2f;
  to.x=101.3f-k*.17f;to.y=12.7f-k*.23f;to.w=3.9f+k*.73f;to.h=29.1f-k*.83f;
  for(i=0;i<3;++i){si[i].window.name=names[i];si[i].window.group=groups[i];si[i].window.flags=(k&1)?0x203e5:0;
   si[i].window.rectClient=to;si[i].window.rectEffects=from;si[i].window.rectEffects2=to;si[i].window.offsetTime=113;
   si[i].textRect.w=13;si[i].textRect.h=17;si[i].parent=parent?&sm:NULL;
   memcpy(oi[i],&si[i].window,sizeof(si[i].window));memcpy(oi[i]+sizeof(si[i].window),&si[i].textRect,sizeof(si[i].textRect));*(void**)(oi[i]+0xec)=parent?&om:NULL;
   sm.items[i]=si+i;om.items[i]=(itemDef_t*)oi[i];
  }
  if(mode){((void(__cdecl*)(menuDef_t*,const char*,float,float,float,float,int))(b+0x16e80))(&om,queries[q],from.x,from.y,to.x,to.y,k*13-9);Menu_OrbitItemByName(&sm,queries[q],from.x,from.y,to.x,to.y,k*13-9);}
  else{((void(__cdecl*)(menuDef_t*,const char*,rectDef_t,rectDef_t,int,float))(b+0x16c40))(&om,queries[q],from,to,k*13-9,amounts[a]);Menu_TransitionItemByName(&sm,queries[q],from,to,k*13-9,amounts[a]);}
  for(i=0;i<3;++i){if(memcmp(&si[i].window,oi[i],sizeof(si[i].window))){printf("motion mismatch mode%d n%d q%d k%d a%d parent%d item%d\n",mode,n,q,k,a,parent,i);fflush(stdout);assert(0);}assert(!memcmp(&si[i].textRect,oi[i]+sizeof(si[i].window),sizeof(si[i].textRect)));}++cases;
 }
 assert(FreeLibrary(h));printf("%d original menu transition/orbit comparisons passed\n",cases);return 0;
}
