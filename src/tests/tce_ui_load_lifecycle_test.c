#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_loadpanel.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
extern void UI_ReadableSize(char*,int,int);
static byte*base;static char trace[8192];static int original,nest,nested,nestedHack,fonts;static float connecting;
static void font(const char*n,int size,fontInfo_t*f){sprintf(trace+strlen(trace),"F:%s:%d:%d;",n,size,fonts++);}
static void setup(panel_button_t**p){strcat(trace,"S;");}
static float value(const char*n){assert(!strcmp(n,"ui_connecting"));strcat(trace,"G;");return connecting;}
static void set(const char*n,const char*v){assert(!strcmp(n,"ui_connecting")&&!strcmp(v,"0"));strcat(trace,"Z;");connecting=0;}
static void render(panel_button_t**p){strcat(trace,"R;");if(nest&&!nested){nested=1;if(original)((void(__cdecl*)(int,int,int))(base+0x37d0))(1,7,nestedHack);else UI_DrawLoadPanel(1,7,nestedHack);}}
static struct{byte*p,saved[5];}hooks[10];static int hc;
static void patch(void*a,void*f){DWORD old,tmp;byte*p=a;assert(hc<10);hooks[hc].p=p;memcpy(hooks[hc++].saved,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old));p[0]=0xe9;*(int*)(p+1)=(byte*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static void restore(void){DWORD old,tmp;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&old));memcpy(hooks[hc].p,hooks[hc].saved,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,old,&tmp);}}
int main(int argc,char**argv){HMODULE h;int init,force,owner,hack,c,pass,cases=0,i,j;char expected[8192],ob[128],sb[128];float values[]={0,1,-.3f};
 const int sizes[]={-2147483647,-1,0,1,1023,1024,1025,1048575,1048576,1048577,1073741823,1073741824,1073741825,1100000000,1500000000,2147483647};
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);base=(byte*)h;
 patch(base+0x20710,font);patch(trap_R_RegisterFont,font);patch(base+0x200c0,setup);patch(BG_PanelButtonsSetup,setup);patch(base+0x200f0,render);patch(BG_PanelButtonsRender,render);patch(base+0x20550,value);patch(trap_Cvar_VariableValue,value);patch(base+0x20530,set);patch(trap_Cvar_Set,set);
 for(init=0;init<2;++init)for(force=0;force<2;++force)for(owner=0;owner<2;++owner)for(hack=0;hack<2;++hack)for(c=0;c<3;++c)for(nest=0;nest<2;++nest)for(nestedHack=0;nestedHack<2;++nestedHack){
  for(pass=0;pass<2;++pass){original=!pass;fonts=0;nested=0;trace[0]=0;connecting=values[c];
   if(original){*(int*)(base+0x54a5c)=init;*(int*)(base+0x54a60)=0;((void(__cdecl*)(int,int,int))(base+0x37d0))(force,owner,hack);assert(*(int*)(base+0x54a60)==0);assert(*(int*)(base+0x54658)==owner);strcpy(expected,trace);}
   else{bg_loadscreeninited=init;UI_DrawLoadPanel(force,owner,hack);assert(connect_ownerdraw==owner);assert(bg_loadscreeninited==1);assert(!strcmp(expected,trace));}
  }++cases;
 }
 for(i=0;i<16;++i)for(j=32;j<=96;j+=32){memset(ob,0xa5,sizeof(ob));memset(sb,0xa5,sizeof(sb));((void(__cdecl*)(char*,int,int))(base+0x12240))(ob,j,sizes[i]);UI_ReadableSize(sb,j,sizes[i]);assert(!memcmp(ob,sb,sizeof(ob)));++cases;}
 restore();assert(FreeLibrary(h));printf("%d original load-panel/readable-size comparisons passed\n",cases);return 0;}
