#define WIN32_LEAN_AND_MEAN
#include "../game/q_shared.h"
#include "../game/bg_public.h"
#include <windows.h>
#include <stdio.h>
#undef _vsnprintf
int Q_vsnprintf(char *s,int n,const char *fmt,va_list ap){return _vsnprintf(s,n,fmt,ap);}
void QDECL Com_Printf(const char *fmt,...){}
void QDECL Com_Error(int code,const char *fmt,...){exit(3);}
static int equal(bg_playerclass_t *a,bg_playerclass_t *b){return a->classNum==b->classNum&&!strcmp(a->characterFile,b->characterFile)&&!strcmp(a->iconName,b->iconName)&&!strcmp(a->iconArrow,b->iconArrow)&&!memcmp(a->classWeapons,b->classWeapons,sizeof(a->classWeapons))&&!memcmp(a->classWeapons2,b->classWeapons2,sizeof(a->classWeapons2));}
int main(int argc,char **argv){unsigned char *b[3];HMODULE h;int m,t,c,w,n=0;bg_playerclass_t *a,*e,probe;playerState_t ps;unsigned char rps[0x800];
 const int tables[]={0xb3068,0x95470,0x290b0},getters[]={0x2af70,0x3640};
 if(argc!=4||sizeof(bg_playerclass_t)!=216)return 2;
 for(m=0;m<3;++m){h=LoadLibraryA(argv[m+1]);if(!h)return 2;b[m]=(unsigned char *)h;
 for(c=0;c<14;++c){a=c<7?bg_allies_playerclasses+c:bg_axis_playerclasses+c-7;e=(bg_playerclass_t *)(b[m]+tables[m]+c*216);if(!equal(a,e)){printf("table mismatch module%d row%d\n",m,c);return 1;}++n;}}
 for(m=0;m<2;++m)for(t=-1;t<5;++t)for(c=-3;c<10;++c){a=BG_GetPlayerClassInfo(t,c);e=((bg_playerclass_t *(__cdecl *)(int,int))(b[m]+getters[m]))(t,c);if(!equal(a,e))return 4;++n;}
 memset(&ps,0,sizeof(ps));memset(rps,0,sizeof(rps));
 for(t=-1;t<5;++t)for(c=-3;c<10;++c){ps.persistant[PERS_TEAM]=*(int *)(rps+0x11c)=t;ps.stats[STAT_PLAYER_CLASS]=*(int *)(rps+0xe4)=c;e=((bg_playerclass_t *(__cdecl *)(void *))(b[0]+0x2afa0))(rps);if(!equal(BG_PlayerClassForPlayerState(&ps),e))return 5;++n;}
 for(t=0;t<4;++t)for(c=0;c<7;++c)for(w=-1;w<68;++w){a=BG_GetPlayerClassInfo(t,c);if(BG_ClassHasWeapon(a,w)!=((int(__cdecl *)(void *,int))(b[0]+0x2afc0))(a,w))return 6;if(BG_WeaponIsPrimaryForClassAndTeam(c,t,w)!=((int(__cdecl *)(int,int,int))(b[0]+0x2aff0))(c,t,w))return 7;++n;}
 /* All 24 slots matter, including entries after a zero and final slot. */
 for(c=0;c<24;++c){memset(&probe,0,sizeof(probe));probe.classWeapons[c]=55;for(w=0;w<57;++w){if(BG_ClassHasWeapon(&probe,w)!=((int(__cdecl *)(void *,int))(b[0]+0x2afc0))(&probe,w))return 8;++n;}}
 for(c=-10;c<20;++c){if(strcmp(BG_ClassnameForNumber(c),((char *(__cdecl *)(int))(b[0]+0x2b050))(c))||strcmp(BG_ClassnameForNumber(c),((char *(__cdecl *)(int))(b[1]+0x3670))(c)))return 9;if(strcmp(BG_ClassLetterForNumber(c),((char *(__cdecl *)(int))(b[1]+0x36d0))(c)))return 10;if(BG_ClassSkillForClass(c)!=((int(__cdecl *)(int))(b[1]+0x3720))(c))return 11;++n;}
 printf("%d original class/equipment comparisons across three modules passed\n",n);return 0;
}
