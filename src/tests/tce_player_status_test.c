#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_draw.c"
#include <windows.h>
#include <stdio.h>
typedef struct { int type;float v[16];char text[40]; } event_t;
static event_t events[64];static int count,aspect,clip,reserve,akimbo,ammoWeapon,modeWeapon,modeCalls,modeArg;
static event_t *ev(int type){event_t*e=&events[count++];memset(e,0,sizeof(*e));e->type=type;return e;}
static void rectEvent(int type,const float*r){memcpy(ev(type)->v,r,16);}
static void cycle1(const float*r,int a,int b,const float*c){rectEvent(1,r);}
static void cycle2(const float*r,int a,int b,const float*c){rectEvent(2,r);}
static void cycle(float bottom){float r[]={770,bottom,60,32};cycle1(r,1,2,NULL);cycle2(r,1,2,NULL);}
static void health(const float*r){rectEvent(3,r);}
static void stamina(const float*r){rectEvent(4,r);}
static void sourceHealth(const float*r,const int*d,const int*s,void*a,void*b){health(r);}
static void sourceStamina(const float*r,int time,int icon,int frame,void*a,void*b,void*c){stamina(r);}
static int ammo(int*a,int*b,int*c){ev(5);*a=clip;*b=reserve;*c=akimbo;return ammoWeapon;}
static int firemode(int w){modeCalls++;modeArg=w;ev(6)->v[0]=w;return modeWeapon;}
static int width(const char*t,float scale,int limit,void*font){event_t*e=ev(7);strcpy(e->text,t);e->v[0]=scale;e->v[1]=limit;return (int)strlen(t)*7;}
static void paint(float x,float y,float sx,float sy,const float*c,const char*t,float adjust,int limit,int style,void*f){event_t*e=ev(8);e->v[0]=x;e->v[1]=y;e->v[2]=sx;e->v[3]=sy;memcpy(e->v+4,c,16);e->v[8]=adjust;e->v[9]=limit;e->v[10]=style;strcpy(e->text,t);}
static void fill(float x,float y,float w,float h,const float*f,const float*s,const float*b,float fraction,int flags){event_t*e=ev(9);e->v[0]=x;e->v[1]=y;e->v[2]=w;e->v[3]=h;memcpy(e->v+4,f,16);memcpy(e->v+8,b,16);e->v[12]=fraction;e->v[13]=flags;}
static void pic(float x,float y,float w,float h,int shader){event_t*e=ev(10);e->v[0]=x;e->v[1]=y;e->v[2]=w;e->v[3]=h;e->v[4]=shader;}
static void color(const float*c){event_t*e=ev(11);e->v[0]=c!=NULL;if(c)memcpy(e->v+1,c,16);}
static int shader(const char*s){if(strstr(s,"ammoframe"))return 101;if(strstr(s,"bombcarrier"))return 102;if(strstr(s,"backpackcarrier"))return 103;if(strstr(s,"VIP"))return 104;if(strstr(s,"hostage"))return 105;if(strstr(s,"Prone"))return 106;if(strstr(s,"Crouch"))return 107;if(strstr(s,"Stand"))return 108;return 999;}
static void cvar(const char*n,char*out,int size){sprintf(out,"%d",aspect);}
static void hook(void*at,void*fn){DWORD o,t;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
int main(int argc,char**argv){HMODULE m;byte*b,raw[0x600];snapshot_t snap;void(__cdecl*original)(void);static event_t expected[64];int k,n;
 if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;b=(byte*)m;original=(void*)(b+0x28530);
 hook(b+0x4e6c0,cycle1);hook(b+0x4e7b0,cycle2);hook(CG_TCEDrawWeaponCycle,cycle);hook(b+0x29100,health);hook(TCE_DrawPlayerHealthMan,sourceHealth);hook(b+0x29460,stamina);hook(TCE_DrawStaminaBar,sourceStamina);
 hook(b+0x20910,width);hook(CG_Text_Width_Ext,width);hook(b+0x20ba0,paint);hook(CG_Text_Paint_Ext,paint);hook(b+0x298b0,fill);hook(CG_FilledBar,fill);hook(b+0x29ea0,pic);hook(CG_DrawPic,pic);hook(b+0x65420,color);hook(trap_R_SetColor,color);hook(trap_R_RegisterShader,shader);hook(trap_R_RegisterShaderNoMip,shader);hook(trap_Cvar_VariableStringBuffer,cvar);
 hook(b+0x28ed0,ammo);hook(CG_PlayerAmmoValue,ammo);hook(b+0x6700,firemode);hook(BG_FiremodeWeapon,firemode);
 *(int*)(b+0x25886cc)=101;*(int*)(b+0x258825c)=102;*(int*)(b+0x2588260)=103;*(int*)(b+0x2588264)=104;*(int*)(b+0x2588268)=105;*(int*)(b+0x2588038)=106;*(int*)(b+0x258803c)=107;*(int*)(b+0x2588040)=108;
 /* Normal CG_Init preloads these cached media handles on both sides. */
 cgs.media.tceAmmoFrame=101;for(k=0;k<4;k++)cgs.media.tcePlayerCarrierIcons[k]=102+k;
 for(k=0;k<3;k++)cgs.media.tceMoveType[k]=106+k;
 for(k=0;k<12000;k++){
  memset(&snap,0,sizeof(snap));memset(raw,0,sizeof(raw));cg.snap=&snap;*(void**)(b+0x40352c4)=raw;
  aspect=*(int*)(b+0x401496c)=k%4;cg.time=*(int*)(b+0x407dea4)=k*7;cg.weaponSelectTime=*(int*)(b+0x4080e44)=cg.time-(k%4==0?500:k%4==1?501:2000);
  clip=k%5==0?0:k%32;reserve=k%7-2;akimbo=k%6-2;ammoWeapon=8;modeWeapon=k%3!=0;
  cg.predictedPlayerState.weapon=*(int*)(b+0x407df64)=k%3==0?4:39;
  weaponDef[8].maxclip=*(int*)(b+0x4c1dff8+8*0x1cc)=30;
  weaponDef[cg.predictedPlayerState.weapon].pump=*(int*)(b+0x4c1e088+cg.predictedPlayerState.weapon*0x1cc)=k%2;
  snap.ps.persistant[10]=*(int*)(raw+0x164)=k%5-1;
  snap.ps.stats[8]=*(int*)(raw+0x11c)=((k&1)?4:0)|((k&2)?256:0);snap.ps.stats[9]=*(int*)(raw+0x120)=((k&4)?256:0)|((k&8)?1024:0)|((k&16)?2:0);
  snap.ps.powerups[PW_REDFLAG]=*(int*)(raw+0x194)=k&32;snap.ps.powerups[PW_BLUEFLAG]=*(int*)(raw+0x198)=k&64;
  snap.ps.pm_flags=*(int*)(raw+0x38)=((k&128)?1:0)|((k&256)?128:0);snap.ps.eFlags=*(int*)(raw+0x94)=(k&512)?0x80000:0;
  snap.ps.serverCursorHint=*(int*)(raw+0x454)=k%4==0?39:k%4==1?10:k%4==2?3:0;snap.ps.weaponTime=*(int*)(raw+0x58)=k%12001;
  count=0;original();n=count;memcpy(expected,events,sizeof(expected));count=0;CG_DrawPlayerStatus();
  if(n!=count || memcmp(expected,events,n*sizeof(event_t))){int j;printf("PlayerStatus case%d events%d/%d\n",k,n,count);for(j=0;j<n&&j<count;j++)if(memcmp(expected+j,events+j,sizeof(event_t))){printf("first event%d type%d/%d xy%.9g,%.9g / %.9g,%.9g text%s/%s\n",j,expected[j].type,events[j].type,expected[j].v[0],expected[j].v[1],events[j].v[0],events[j].v[1],expected[j].text,events[j].text);break;}return 1;}
 }puts("12000 whole PlayerStatus controller comparisons passed");return 0;
}
