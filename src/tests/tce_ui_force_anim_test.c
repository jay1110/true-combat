#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_players.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
int main(int argc,char**argv){HMODULE h;unsigned char*b;int mode,anim,old,t,cases=0;playerInfo_t p;unsigned char original[0x5870],expected[0x5870];int prior[]={0,1,0x200,0x201,-1};int timers[]={-1,0,777};assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;
assert(LEGS_JUMP==112&&TORSO_GESTURE==47&&TORSO_ATTACK==53&&TORSO_ATTACK2==60);
for(mode=0;mode<2;++mode)for(anim=-1;anim<=1025;++anim)for(old=0;old<5;++old)for(t=0;t<3;++t){memset(&p,0x35,sizeof(p));memset(original,0x35,sizeof(original));p.legsAnim=p.torsoAnim=prior[old];p.legsAnimationTimer=p.torsoAnimationTimer=timers[t];*(int*)(original+0x5808)=*(int*)(original+0x580c)=prior[old];*(int*)(original+0x582c)=*(int*)(original+0x5824)=timers[t];memcpy(expected,original,sizeof(original));((void(__cdecl*)(void*,int))(b+(mode?0x146b0:0x14680)))(original,anim);if(mode)UI_ForceTorsoAnim(&p,anim);else UI_ForceLegsAnim(&p,anim);*(int*)(expected+0x5808)=p.legsAnim;*(int*)(expected+0x580c)=p.torsoAnim;*(int*)(expected+0x582c)=p.legsAnimationTimer;*(int*)(expected+0x5824)=p.torsoAnimationTimer;if(memcmp(original,expected,sizeof(original))){fprintf(stderr,"force anim mismatch mode%d anim%d old%d timer%d\n",mode,anim,old,t);return 1;}++cases;}
printf("%d original UI force-animation comparisons passed\n",cases);FreeLibrary(h);return 0;}
