#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_shared.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
static char trace[8192];
static void stop(int h){sprintf(trace+strlen(trace),"C%d;",h);}
static void music(const char*a,const char*b,int f){sprintf(trace+strlen(trace),"M:%s:%s:%d;",a,b,f);}
static void exec(int w,const char*s){sprintf(trace+strlen(trace),"E%d:%s;",w,s);}
int main(int argc,char**argv){HMODULE h;byte*b;static displayContextDef_t dc;static byte odc[0x20100];static itemDef_t si[3][3];static byte oi[3][3][0x280];menuDef_t*om;char expected[8192];int mode,n,k,a,script,sound,i,j,cases=0;const char*scripts[]={NULL,"","exec \"opened\""};const char*sounds[]={NULL,"","sound/music/menu.wav"};
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;assert(sizeof(menuDef_t)==0x718);om=(menuDef_t*)(b+0x255da0);
 dc.stopCinematic=stop;dc.startBackgroundTrack=music;dc.executeText=exec;Init_Display(&dc);*(void**)(odc+0xf0)=stop;*(void**)(odc+0xe4)=music;*(void**)(odc+0xcc)=exec;*(void**)(b+0x245fa4)=odc;String_Init();((void(__cdecl*)(void))(b+0x14af0))();
 for(mode=0;mode<2;++mode)for(n=0;n<=3;++n)for(k=0;k<12;++k)for(a=0;a<3;++a)for(script=0;script<3;++script)for(sound=0;sound<3;++sound){
  memset(Menus,0,3*sizeof(menuDef_t));memset(om,0,3*sizeof(menuDef_t));memset(si,0,sizeof(si));memset(oi,0,sizeof(oi));menuCount=*(int*)(b+0x245fb8)=n;dc.realTime=*(int*)(odc+0x120)=k*71-100;
  for(i=0;i<3;++i){Menus[i].window.flags=0x20000000|(k&7);Menus[i].window.style=(k+i)%2?WINDOW_STYLE_CINEMATIC:WINDOW_STYLE_FILLED;Menus[i].window.cinematic=(k+i)%3-1;Menus[i].itemCount=k%4;Menus[i].onOpen=scripts[script];Menus[i].soundName=sounds[sound];om[i]=Menus[i];
   for(j=0;j<3;++j){si[i][j].window.style=(k+j)%2?WINDOW_STYLE_CINEMATIC:WINDOW_STYLE_FILLED;si[i][j].window.cinematic=(k+j)%3-1;si[i][j].window.ownerDraw=j*7;si[i][j].type=j==a?ITEM_TYPE_OWNERDRAW:ITEM_TYPE_TEXT;memcpy(oi[i][j],&si[i][j].window,sizeof(windowDef_t));*(int*)(oi[i][j]+0xc8)=si[i][j].type;Menus[i].items[j]=&si[i][j];om[i].items[j]=(itemDef_t*)oi[i][j];}
  }
  trace[0]=0;if(mode)((void(__cdecl*)(menuDef_t*))(b+0x19f80))(om+a);else((void(__cdecl*)(menuDef_t*))(b+0x1a050))(n?om+a:NULL);strcpy(expected,trace);
  trace[0]=0;if(mode)Menus_Activate(Menus+a);else Menu_CloseCinematics(n?Menus+a:NULL);assert(!strcmp(trace,expected));
  for(i=0;i<3;++i){assert(!memcmp(&Menus[i].window,&om[i].window,sizeof(windowDef_t)));assert(Menus[i].openTime==om[i].openTime);for(j=0;j<3;++j)assert(!memcmp(&si[i][j].window,oi[i][j],sizeof(windowDef_t)));}++cases;
 }
 assert(FreeLibrary(h));printf("%d original menu activation/cinematic comparisons passed\n",cases);return 0;}
