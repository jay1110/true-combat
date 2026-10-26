#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include "../game/tce_trajectory.h"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
static byte *base,rps[0x1000],res[0x200];
static playerState_t ps;static entityState_t es;static jmp_buf jump;
static int side,bad,calls,variant;static float pos[3];static int skills[SK_NUM_SKILLS];
typedef struct {int result,calls,code;char text[160];} outcome;
static outcome out;
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static void QDECL fatal(int code,const char*text,...){out.code=code;Q_strncpyz(out.text,text,sizeof(out.text));longjmp(jump,1);}
static int magic(void*p,int*s,int team,int clips){calls++;if(p!=(side?(void*)&ps:(void*)rps)||s!=skills||team!=variant-7||clips!=0)bad=1;return variant%3-1;}
static void trajectory(void*tr,int time,float*result,int angle,int spline,float scale){calls++;if(tr!=(side?(void*)&es.pos:(void*)(res+12))||time!=10000+variant||angle||spline!=variant-5||scale!=1)bad=1;memcpy(result,pos,sizeof(pos));}
int main(int argc,char**argv){HMODULE dll;int (__cdecl*grab)(void*,void*,int*,int);int(__cdecl*touch)(void*,void*,int);int type,tag,v,s,n=0,t=0,index;outcome expected;float values[]={-100,-36.000004f,-36,-35.999996f,0,35.999996f,36,36.000004f,100};int x,y,z;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;grab=(void*)(base+0x2ba60);touch=(void*)(base+0x2b510);
 if(!hook(base+0x61860,fatal)||!hook(Com_Error,fatal)||!hook(base+0x2b640,magic)||!hook(BG_AddMagicAmmo,magic)||!hook(base+0x2c2e0,trajectory)||!hook(TCE_BG_EvaluateTrajectory,trajectory))return 2;
 /* Mutate the input catalogue entry on both sides; the controller's complete
  * switch, including currently unused/invalid classes, is thereby exercised. */
 for(type=-1;type<=10;type++)for(tag=0;tag<=13;tag++)for(v=0;v<256;v++)for(s=0;s<2;s++){
 side=s;variant=v;calls=bad=0;memset(&out,0,sizeof(out));out.result=-99;memset(&ps,0,sizeof(ps));memset(&es,0,sizeof(es));memset(rps,0,sizeof(rps));memset(res,0,sizeof(res));
 index=(v%32==0)?-1:(v%32==1)?bg_numItems:(v%32==2)?0:1;
 es.modelindex=*(int*)(res+0xa4)=index;
 es.density=*(int*)(res+0xf4)=v%5-2;es.otherEntityNum2=*(int*)(res+0x90)=v%4;
 ps.persistant[PERS_TEAM]=*(int*)(rps+0x11c)=(v/4)%4;
 ps.stats[STAT_TCE_FLAGS]=*(int*)(rps+0xf4)=(v/16)*0x100;
 ps.stats[STAT_HEALTH]=*(int*)(rps+0xd0)=v%3*100;ps.stats[STAT_MAX_HEALTH]=*(int*)(rps+0xe0)=100;
 bg_itemlist[1].giType=type;bg_itemlist[1].giTag=tag;*(int*)(base+0xb6b5c+60)=type;*(int*)(base+0xb6b60+60)=tag;
 if(!setjmp(jump))out.result=s?BG_CanItemBeGrabbed(&es,&ps,skills,v-7):grab(res,rps,skills,v-7);out.calls=calls;
 if(!s)expected=out;else{if(bad||memcmp(&out,&expected,sizeof(out))){printf("grab mismatch type%d tag%d variant%d result%d/%d calls%d/%d fatal%d/%d\n",type,tag,v,expected.result,out.result,expected.calls,out.calls,expected.code,out.code);return 1;}n++;}}
 for(v=0;v<8;v++)for(x=0;x<10;x++)for(y=0;y<10;y++)for(z=0;z<10;z++)for(s=0;s<2;s++){
 side=s;variant=v;calls=bad=0;memset(&out,0,sizeof(out));memset(&ps,0,sizeof(ps));memset(&es,0,sizeof(es));memset(rps,0,sizeof(rps));memset(res,0,sizeof(res));
 pos[0]=pos[1]=pos[2]=0;ps.origin[0]=x==9?0:values[x];ps.origin[1]=y==9?0:values[y];ps.origin[2]=z==9?0:values[z];
 if(x==9)*(unsigned*)&ps.origin[0]=0x7fc00000;if(y==9)*(unsigned*)&ps.origin[1]=0x7fc00000;if(z==9)*(unsigned*)&ps.origin[2]=0x7fc00000;
 memcpy(rps+0x14,ps.origin,12);es.effect2Time=*(int*)(res+0x110)=v-5;
 out.result=s?BG_PlayerTouchesItem(&ps,&es,10000+v):touch(rps,res,10000+v);out.calls=calls;
 if(!s)expected=out;else{if(bad||memcmp(&out,&expected,sizeof(out))){printf("touch mismatch v%d xyz%d/%d/%d result%d/%d\n",v,x,y,z,expected.result,out.result);return 1;}t++;}}
 printf("PASS item eligibility %d grab and %d touch cases\n",n,t);return 0;
}
