#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
typedef struct { int type,a,b;float values[4]; } event_t;
static event_t events[8];static int count,nextHint,nullFade;static byte *originalBase;static int originalRun;static float fadeColor[4]={.17f,.39f,.63f,.81f};static int specialHandles[2];
static event_t *event(int type){event_t *e=&events[count++];memset(e,0,sizeof(*e));e->type=type;return e;}
static void check(void){event(1);if(originalRun)*(int*)(originalBase+0x4080dcc)=nextHint;else cg.cursorHintIcon=nextHint;}
static float *fade(int time,int duration){event_t*e=event(2);e->a=time;e->b=duration;return nullFade?NULL:fadeColor;}
static void color(const float *c){event_t*e=event(3);e->a=c!=NULL;if(c)memcpy(e->values,c,16);}
static void pic(float x,float y,float w,float h,int shader){event_t*e=event(4);e->a=shader;e->values[0]=x;e->values[1]=y;e->values[2]=w;e->values[3]=h;}
static int shader(const char *s){if(!strcmp(s,"gfx/2d/ledgeHint"))return specialHandles[0];if(!strcmp(s,"gfx/2d/objLockedHint"))return specialHandles[1];return -999;}
static void hook(void*at,void*fn){DWORD old,t;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t);}
int main(int argc,char **argv){HMODULE module;byte*b;void(__cdecl *original)(rectDef_t*);event_t expected[8];int initial,produced,mode,variant,k=0,i,n;rectDef_t rect;
 if(argc!=2||!(module=LoadLibraryA(argv[1])))return 2;b=originalBase=(byte*)module;original=(void*)(b+0x4ec50);
 hook(b+0x21210,check);hook(CG_CheckForCursorHints,check);hook(b+0x2a730,fade);hook(CG_FadeColor,fade);hook(b+0x65420,color);hook(trap_R_SetColor,color);hook(b+0x29ea0,pic);hook(CG_DrawPic,pic);hook(trap_R_RegisterShader,shader);
 for(initial=-2;initial<55;initial++)for(produced=-2;produced<55;produced++)for(mode=0;mode<5;mode++)for(variant=0;variant<4;variant++,k++){
  /* Same registration handles; original contiguous group usable..dynamite. */
  for(i=0;i<36;i++){int handle=variant&2?0:100+i;(&cgs.media.usableHintShader)[i]=handle;*(int*)(b+0x258819c+4*i)=handle;}
  cgs.media.tankHintShader=*(int*)(b+0x25882a0)=variant&2?0:201;cgs.media.satchelchargeHintShader=*(int*)(b+0x25882a4)=variant&2?0:202;
  cgs.media.uniformHintShader=*(int*)(b+0x25882a8)=variant&2?0:203;cgs.media.landmineHintShader=*(int*)(b+0x2588324)=variant&2?0:204;
  specialHandles[0]=*(int*)(b+0x258822c)=variant&2?0:205;specialHandles[1]=*(int*)(b+0x2588230)=variant&2?0:206;
  nextHint=produced;nullFade=variant&1;cg_cursorHints.integer=*(int*)(b+0x4012f8c)=mode;
  cg.cursorHintTime=*(int*)(b+0x4080dd0)=k-123;cg.cursorHintFade=*(int*)(b+0x4080dd4)=k%1001-1;
  rect.x=(k%1000-200)/7.f;rect.y=(k%701-100)/11.f;rect.w=(k%91-10)/3.f;rect.h=(k%67-5)/9.f;
  cg.cursorHintIcon=*(int*)(b+0x4080dcc)=initial;count=0;originalRun=1;original(&rect);n=count;memcpy(expected,events,sizeof(expected));
  count=0;originalRun=0;CG_DrawCursorhint(&rect);
  if(n!=count||memcmp(expected,events,n*sizeof(event_t))||cg.cursorHintIcon!=*(int*)(b+0x4080dcc)){printf("cursor hint case%d initial%d produced%d mode%d variant%d events%d/%d\n",k,initial,produced,mode,variant,n,count);return 1;}
 }printf("%d whole cursor hint comparisons passed\n",k);return 0;}
