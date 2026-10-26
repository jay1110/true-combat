#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
extern gentity_t *SelectRandomTeamSpawnPoint(int,team_t,int,int,qboolean);
static byte *base,raw[258][0x57c];static gentity_t spots[258];
static int side,variant,count,available,bad;static jmp_buf fatalJump;
typedef struct{int k,a,b;char text[40];}event;static event events[1600],expectedEvents[1600];
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static int indexOf(void*p){int i;if(!p)return -1;for(i=0;i<258;i++)if(p==(side?(void*)&spots[i]:(void*)raw[i]))return i;bad=1;return -2;}
static void note(int k,int a,int b,const char*s){event*e;if(count>=1600)exit(3);e=&events[count++];memset(e,0,sizeof(*e));e->k=k;e->a=a;e->b=b;if(s)Q_strncpyz(e->text,s,sizeof(e->text));}
static void*find(void*previous,int offset,const char*name){int i=indexOf(previous)+1;if(offset!=(side?FOFS(classname):0x1a4))bad=1;note(1,i-1,0,name);if(i>=available)return NULL;return side?(void*)&spots[i]:(void*)raw[i];}
static int telefrag(void*p){int i=indexOf(p),v=(variant/6)%4;note(2,i,0,NULL);return v==1?1:v==2?(i%2):0;}
/* The original comparison covers the unchanged clear-world selector policy.
 * Only the new production admission helper is held clear; keep its trace
 * outside the original event stream. Blocked-world behavior has its own native fixture. */
static qboolean allClearWorld(const vec3_t origin){(void)origin;return qtrue;}
static void*fallback(const float*avoid,float*origin,float*angles){int i;note(3,0,0,NULL);for(i=0;i<3;i++){if(avoid[i]!=0)bad=1;origin[i]=91+i;angles[i]=81+i;}return side?(void*)&spots[257]:(void*)raw[257];}
static void QDECL fatal(const char*f,...){va_list a;char s[40];va_start(a,f);Q_vsnprintf(s,sizeof(s),f,a);va_end(a);note(4,0,0,s);longjmp(fatalJump,1);}
int main(int argc,char**argv){HMODULE dll;void*(__cdecl*original)(int,int,int,int,int);void*(__cdecl*wrapper)(int,int,float*,float*,int,int,int);int v,t,h,w,i,j,total=0,ret,wantRet,fatalSeen,wantFatal,wantCount,objective,preferred;float origin[3],angles[3],wantOrigin[3],wantAngles[3];int sizes[]={0,1,2,8,256,257};int modes[]={0,2,5,7};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;original=(void*)(base+0x8df60);wrapper=(void*)(base+0x8e2c0);
#define H(r,fn,repl) if(!hook(base+r,repl)||!hook(fn,repl))return 2
 H(0x948e0,G_Find,find);H(0x4aaa0,SpotWouldTelefrag,telefrag);H(0x4acc0,SelectSpawnPoint,fallback);H(0x5ee10,G_Error,fatal);
 if(!hook(G_SpawnOriginWorldClear,allClearWorld))return 2;
 for(v=0;v<768;v++)for(t=0;t<4;t++)for(h=0;h<2;h++)for(w=0;w<2;w++){
  /* Original hostage/team0 requires an explicit target: target[-1] is not a
   * valid objective. Other teams cover the auto-target branch. */
  objective=(t==0||t==3)?1:(v/24)%3;preferred=(v/72)%4==0?0:(v/72)%4==1?101:(v/72)%4==2?-1:999;
  for(side=0;side<2;side++){
   variant=v;available=sizes[v%6];count=bad=fatalSeen=0;ret=-9;memset(events,0,sizeof(events));for(i=0;i<3;i++){origin[i]=77+i;angles[i]=88+i;}
   level.numspawntargets=*(int*)(base+0x7ffdfc)=(v/96)%2?0:2;level.axisAutoSpawn=*(int*)(base+0x222232c)=0;level.alliesAutoSpawn=*(int*)(base+0x2222330)=1;level.tceSpawnPhase=*(int*)(base+0x2223930)=(v/48)%5-1;g_gametype.integer=*(int*)(base+0x6d8d4c)=modes[(v/192)%4];
   for(i=0;i<2;i++)for(j=0;j<3;j++)level.spawntargets[i][j]=*(float*)(base+0x7ffd3c+i*12+j*4)=i?100.f:0.f;
   for(i=0;i<258;i++){memset(&spots[i],0,sizeof(spots[i]));memset(raw[i],0,0x57c);spots[i].s.number=*(int*)raw[i]=100+i;spots[i].spawnflags=*(int*)(raw[i]+0x1a8)=((v/12)%3==1?0:2);spots[i].entstate=*(int*)(raw[i]+0x47c)=((v/18)%3==1?(i%3):(v/18)%3==2?STATE_LOCKED:STATE_DEFAULT);spots[i].tceObjectiveScore=*(int*)(raw[i]+0x2e4)=(v/36)%3==0?0:(v/36)%3==1?(i%5)-2:level.tceSpawnPhase;
    for(j=0;j<3;j++){float x=(v<12)?(j==0?1.f:j==1?2.f:0.f):(float)((i*17+j*31+v)%1000)*.125f;spots[i].s.origin[j]=*(float*)(raw[i]+0x5c+j*4)=x;spots[i].s.angles[j]=*(float*)(raw[i]+0x74+j*4)=i+j*.5f;}
   }
   if(!setjmp(fatalJump)){void*p;if(w)p=side?(void*)SelectCTFSpawnPoint(t,1,origin,angles,objective,preferred,h):wrapper(t,1,origin,angles,objective,preferred,h);else p=side?(void*)SelectRandomTeamSpawnPoint(1,t,objective,preferred,h):original(1,t,objective,preferred,h);ret=indexOf(p);}else fatalSeen=1;
   if(!side){wantRet=ret;wantFatal=fatalSeen;wantCount=count;memcpy(expectedEvents,events,sizeof(events));memcpy(wantOrigin,origin,12);memcpy(wantAngles,angles,12);}else{if(bad||ret!=wantRet||fatalSeen!=wantFatal||count!=wantCount||memcmp(events,expectedEvents,sizeof(events))||memcmp(origin,wantOrigin,12)||memcmp(angles,wantAngles,12)){printf("Spawn selection case%d team%d hostage%d wrapper%d bad%d ret%d/%d fatal%d/%d calls%d/%d\n",v,t,h,w,bad,wantRet,ret,wantFatal,fatalSeen,wantCount,count);for(i=0;i<count||i<wantCount;i++)if(memcmp(&events[i],&expectedEvents[i],sizeof(event))){printf("call%d %d,%d,%d[%s] / %d,%d,%d[%s]\n",i,expectedEvents[i].k,expectedEvents[i].a,expectedEvents[i].b,expectedEvents[i].text,events[i].k,events[i].a,events[i].b,events[i].text);break;}return 1;}total++;}
  }
 }
 printf("PASS whole team-spawn selector and wrapper %d cases (6144 each)\n",total);return 0;
}
