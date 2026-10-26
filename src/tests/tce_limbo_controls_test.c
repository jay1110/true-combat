#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_consolecmds.c"
#include <windows.h>
#include <stdio.h>
extern qboolean CG_LimboPanel_WeaponLights_KeyDown(panel_button_t *,int);
static int team,n,events[16][3];
static char config[256];
static void event(int id,int a,int c){events[n][0]=id;events[n][1]=a;events[n++][2]=c;}
static int getTeam(void){event(1,0,0);return team;}
static const char *getConfig(int index){event(2,index,0);return config;}
static void sound(int s,int channel){event(3,s,channel);}
static void request(void){event(4,0,0);}
static void hook(void*at,void*fn){DWORD old,t;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t);}
int main(int argc,char**argv){HMODULE m;byte*b;int(__cdecl*maxfn)(void);int(__cdecl*lights)(void*,int);void(__cdecl*toggle)(void);int k,op,r,s,count,copy[16][3],key;panel_button_t button;byte raw[256];
 const char*values[]={"0","1","5","-1","  +12tail","0x10","","27"};
 if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;b=(byte*)m;maxfn=(void*)(b+0x40dc0);lights=(void*)(b+0x41a20);toggle=(void*)(b+0x217b0);
 hook(b+0x434f0,getTeam);hook(CG_LimboPanel_GetTeam,getTeam);hook(b+0x481a0,getConfig);hook(CG_ConfigString,getConfig);hook(b+0x65070,sound);hook(trap_S_StartLocalSound,sound);hook(b+0x43b20,request);hook(CG_LimboPanel_RequestWeaponStats,request);
 for(k=0;k<2048;k++)for(op=0;op<3;op++){
  memset(&button,0,sizeof(button));memset(raw,0,sizeof(raw));team=k%5;key=(k/5)%3==0?K_MOUSE1:((k/5)%3==1?K_MOUSE2:0);button.data[0]=*(int*)(raw+24)=k%9-3;
  snprintf(config,sizeof(config),"\\unrelated\\77\\numobjectives\\%s\\tail\\8",values[(k/15)%8]);
  cgs.media.sndLimboSelect=*(int*)(b+0x2588dd8)=k-900;cgs.ccSelectedWeaponNumber=*(int*)(b+0x3fde380)=99;cg.tceShowObjectiveDesc=*(int*)(b+0x40a49e0)=k%9-4;
  n=0;if(op==0)r=maxfn();else if(op==1)r=lights(raw,key);else{toggle();r=0;}count=n;memcpy(copy,events,sizeof(copy));
  n=0;if(op==0)s=CG_LimboPanel_GetMaxObjectives();else if(op==1)s=CG_LimboPanel_WeaponLights_KeyDown(&button,key);else{CG_ToggleObjectiveDesc_f();s=0;}
  if(r!=s||n!=count||memcmp(copy,events,n*sizeof(events[0]))||cgs.ccSelectedWeaponNumber!=*(int*)(b+0x3fde380)||cg.tceShowObjectiveDesc!=*(int*)(b+0x40a49e0)){printf("limbo controls case%d op%d result%d/%d calls%d/%d\n",k,op,r,s,count,n);return 1;}
 }puts("6144 whole limbo/objective control comparisons passed (2048 each)");return 0;}
