#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <setjmp.h>
static jmp_buf fatalJump;
static char fatalText[1024];
static void captureFatal(const char *text){Q_strncpyz(fatalText,text,sizeof(fatalText));longjmp(fatalJump,1);}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
extern void CG_SetHudHeadLerpFrameAnimation(bg_character_t*,lerpFrame_t*,int);
extern void CG_ClearHudHeadLerpFrame(bg_character_t*,lerpFrame_t*,int);
extern void CG_RunHudHeadLerpFrame(bg_character_t*,lerpFrame_t*,int,float);
static int same(byte*r,lerpFrame_t*s,byte*ch,bg_character_t*sc){int idx=(int)(((byte*)*(void**)(r+0x34)-(ch+0x108))/0x70);
 return *(int*)r==s->oldFrame&&*(int*)(r+4)==s->oldFrameTime&&*(int*)(r+8)==s->oldFrameModel&&*(int*)(r+12)==s->frame&&*(int*)(r+16)==s->frameTime&&*(int*)(r+20)==s->frameModel&&fabs(*(float*)(r+24)-s->backlerp)<0.000001f&&*(int*)(r+44)==s->animationNumber&&*(int*)(r+56)==s->animationTime&&s->animation==&sc->hudheadanimations[idx];}
int main(int argc,char**argv){HMODULE dll;byte*b;static byte ch[4096],lf[256];static bg_character_t sc;static lerpFrame_t sl;int c,j,op,n=0;void(__cdecl*set)(void*,void*,int);void(__cdecl*clear)(void*,void*,int);void(__cdecl*run)(void*,void*,int,float);
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;set=(void*)(b+0x566b0);clear=(void*)(b+0x56700);run=(void*)(b+0x56740);
 for(c=0;c<12000;++c){int number=(c%16)|((c&16)?512:0);float speed=(float)(c%5)*0.5f;
  for(j=0;j<16;++j){byte*a=ch+0x108+j*0x70;animation_t*s=&sc.hudheadanimations[j];*(int*)a=s->mdxFile=j+51;*(int*)(a+68)=s->firstFrame=j*13;*(int*)(a+72)=s->numFrames=12;*(int*)(a+76)=s->loopFrames=c%3?4:0;*(int*)(a+80)=s->frameLerp=c%7?40:0;*(int*)(a+84)=s->initialLerp=c%401;}
  for(op=0;op<3;++op){memset(lf,0,sizeof(lf));memset(&sl,0,sizeof(sl));cg.time=*(int*)(b+0x407dea4)=1000+c%503;*(int*)(lf+16)=sl.frameTime=900;*(int*)(lf+4)=sl.oldFrameTime=850;
   if(op==0){set(ch,lf,number);CG_SetHudHeadLerpFrameAnimation(&sc,&sl,number);}else if(op==1){clear(ch,lf,number);CG_ClearHudHeadLerpFrame(&sc,&sl,number);}else{if(c%3){clear(ch,lf,number);CG_ClearHudHeadLerpFrame(&sc,&sl,number);}cg.time=*(int*)(b+0x407dea4)+=c%1300;run(ch,lf,(c&1)?number:((number+1)&15),speed);CG_RunHudHeadLerpFrame(&sc,&sl,(c&1)?number:((number+1)&15),speed);}
   if(!same(lf,&sl,ch,&sc)){printf("head case%d operation%d\n",c,op);return 1;}++n;
  }
 }
 for(c=0;c<500;++c){memset(lf,0,sizeof(lf));memset(&sl,0,sizeof(sl));cg.time=*(int*)(b+0x407dea4)=1000;clear(ch,lf,0);CG_ClearHudHeadLerpFrame(&sc,&sl,0);*(int*)(lf+16)=sl.frameTime=1001+c;*(int*)(lf+4)=sl.oldFrameTime=999+c;run(ch,lf,0,1);CG_RunHudHeadLerpFrame(&sc,&sl,0,1);if(!same(lf,&sl,ch,&sc)){printf("future frame case%d\n",c);return 1;}++n;}
 hook(b+0x64ba0,captureFatal);hook(trap_Error,captureFatal);
 for(c=0;c<12;++c)for(op=0;op<3;++op){static const int invalid[]={-1,-512,-513,16,17,511,528,1024,65535,2147483647,(-2147483647-1),-16};static char expectedError[1024];int number=invalid[c];
  memset(lf,0,sizeof(lf));memset(&sl,0,sizeof(sl));cg.time=*(int*)(b+0x407dea4)=12345;
  *(int*)(lf+4)=sl.oldFrameTime=71;*(int*)(lf+16)=sl.frameTime=72;*(int*)(lf+56)=sl.animationTime=73;
  fatalText[0]=0;
  if(!setjmp(fatalJump)){if(op==0)set(ch,lf,number);else if(op==1)clear(ch,lf,number);else run(ch,lf,number,1);printf("original failed to reject %d operation%d\n",number,op);return 1;}
  strcpy(expectedError,fatalText);fatalText[0]=0;
  if(!setjmp(fatalJump)){if(op==0)CG_SetHudHeadLerpFrameAnimation(&sc,&sl,number);else if(op==1)CG_ClearHudHeadLerpFrame(&sc,&sl,number);else CG_RunHudHeadLerpFrame(&sc,&sl,number,1);printf("source failed to reject %d operation%d\n",number,op);return 1;}
  if(strcmp(expectedError,fatalText)||*(int*)(lf+44)!=sl.animationNumber||*(int*)(lf+4)!=sl.oldFrameTime||*(int*)(lf+16)!=sl.frameTime||*(int*)(lf+56)!=sl.animationTime||*(void**)(lf+52)!=NULL||sl.animation!=NULL){printf("fatal head %d operation%d: %s/%s\n",number,op,expectedError,fatalText);return 1;}++n;
 }
 printf("%d whole HUD head lerp comparisons passed (including36 fatal cases)\n",n);return 0;
}

