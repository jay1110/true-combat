#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
static byte*raw;static gentity_t*ent;static int mode,bad,events[8],count,mutation,fatal,thinkSeen;static jmp_buf escape;
static int hook(void*p,void*f){DWORD old,tmp;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);return 1;}
static void*active(void){return mode?(void*)raw:(void*)ent;}
static int script(gentity_t*e){events[count++]=1;if(e!=active())bad=1;if(mutation>=0){int value=mutation==0?0:mutation==1?level.time-1:mutation==2?level.time:level.time+1;if(mode)*(int*)(raw+0x29c)=value;else ent->nextthink=value;}return 1;}
static void think(gentity_t*e){events[count++]=2;if(e!=active()||(mode?*(int*)(raw+0x29c):ent->nextthink)!=0)bad=1;++thinkSeen;}
static void error(const char*text){events[count++]=3;if(strcmp(text,"NULL ent->think"))bad=1;++fatal;longjmp(escape,1);}
int main(int argc,char**argv){HMODULE dll;byte*b;void(__cdecl*original)(void*);int ix,num,pause,cls,ti,ni,th,n=0;int indices[]={0,31,63,64,100},times[]={0,1,100,16777215,16777216,16777217,33554433};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x63b80);
 if(!hook(b+0x7d4e0,script)||!hook(G_Script_ScriptRun,script)||!hook(b+0x8ab30,error)||!hook(trap_Error,error))return 2;
 for(ix=0;ix<5;++ix)for(num=0;num<2;++num)for(pause=0;pause<2;++pause)for(cls=0;cls<2;++cls)for(ti=0;ti<7;++ti)for(ni=0;ni<8;++ni)for(mutation=-1;mutation<4;++mutation)for(th=0;th<2;++th){int expectedEvents[8],expectedCount,expectedFatal,expectedThink,expectedNext;int nexts[]={-1,0,1,99,100,times[ti]-1,times[ti],times[ti]+1};
  raw=b+0x226e2a0+indices[ix]*0x57c;ent=&g_entities[indices[ix]];memset(raw,0,0x57c);memset(ent,0,sizeof(*ent));ent->s.number=*(int*)raw=num?64:63;ent->classname=*(char**)(raw+0x1a4)=cls?"prefixDPRINTF_test":"normal";ent->nextthink=*(int*)(raw+0x29c)=nexts[ni];ent->think=th?think:NULL;*(void**)(raw+0x2a4)=ent->think;
  level.time=*(int*)(b+0x7febe0)=times[ti];level.previousTime=*(int*)(b+0x7febe4)=times[ti]-17;level.match_pause=*(int*)(b+0x221bafc)=pause;g_maxclients.integer=*(int*)(b+0x7e18ac)=64;
  mode=1;bad=count=fatal=thinkSeen=0;if(setjmp(escape)==0)original(raw);if(bad){fprintf(stderr,"original think callback mismatch case%d\n",n);return 3;}expectedCount=count;memcpy(expectedEvents,events,sizeof(events));expectedFatal=fatal;expectedThink=thinkSeen;expectedNext=*(int*)(raw+0x29c);
  mode=0;bad=count=fatal=thinkSeen=0;if(setjmp(escape)==0)G_RunThink(ent);
  if(bad||count!=expectedCount||memcmp(events,expectedEvents,count*sizeof(int))||fatal!=expectedFatal||thinkSeen!=expectedThink||ent->nextthink!=expectedNext){fprintf(stderr,"think case%d ix%d num%d pause%d class%d time%d next%d mutation%d handler%d mismatch next%d/%d events%d/%d fatal%d/%d\n",n,indices[ix],num,pause,cls,times[ti],nexts[ni],mutation,th,ent->nextthink,expectedNext,count,expectedCount,fatal,expectedFatal);return 1;}++n;
 }
 printf("%d whole G_RunThink comparisons passed including NULL-think fatal capture\n",n);return 0;
}
