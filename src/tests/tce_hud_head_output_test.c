#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
#include <math.h>
extern void CG_HudHeadAnimation(bg_character_t*,lerpFrame_t*,int*,int*,float*,int);
int main(int argc,char**argv){HMODULE dll;byte*b;static byte ch[4096],lf[256];static bg_character_t sc;lerpFrame_t sl;int c,j,n=0;void(__cdecl*original)(void*,void*,int*,int*,float*,int);
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x56870);
 for(c=0;c<12000;++c){int number=(c%16)|((c&16)?512:0),eo=-999,ef=-998,so=-999,sf=-998;float eb=-997,sb=-997;
  for(j=0;j<16;++j){byte*a=ch+0x108+j*0x70;animation_t*s=&sc.hudheadanimations[j];*(int*)a=s->mdxFile=j+51;*(int*)(a+68)=s->firstFrame=j*13;*(int*)(a+72)=s->numFrames=12;*(int*)(a+76)=s->loopFrames=c%3?4:0;*(int*)(a+80)=s->frameLerp=c%7?40:0;*(int*)(a+84)=s->initialLerp=c%401;}
  memset(lf,0,sizeof(lf));memset(&sl,0,sizeof(sl));cg.time=*(int*)(b+0x407dea4)=1000;
  original(ch,lf,&eo,&ef,&eb,number);CG_HudHeadAnimation(&sc,&sl,&so,&sf,&sb,number);
  for(j=0;j<4;++j){cg.time=*(int*)(b+0x407dea4)=1000+(c%1500)*j;original(ch,lf,&eo,&ef,&eb,number);CG_HudHeadAnimation(&sc,&sl,&so,&sf,&sb,number);if(eo!=so||ef!=sf||fabs(eb-sb)>0.000001f||so!=sl.oldFrame||sf!=sl.frame||sb!=sl.backlerp){printf("head output case%d step%d\n",c,j);return 1;}++n;}
 }
 printf("%d HUD-head output comparisons passed (plus12000 initialization calls)\n",n);return 0;
}
