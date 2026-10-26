#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
static byte raw[0x57c],rawClient[0x2000],rawInf[0x57c],rawAtt[0x57c];static gentity_t ent,inf,att;static gclient_t client;static int calls,mode,bad,parmSeen;
extern void AddKillScore(gentity_t*,int);extern void LookAtKiller(gentity_t*,gentity_t*,gentity_t*);extern void GibEntity(gentity_t*,int);
static int hook(void*p,void*f){DWORD old,tmp;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);return 1;}
static void ranks(void){++calls;}
static void gibEvent(gentity_t*e,int ev,int parm){++calls;if(e!=(mode?(void*)raw:&ent)||ev!=(mode?75:EV_GIB_PLAYER))bad=1;parmSeen=parm;}
int main(int argc,char**argv){HMODULE dll;byte*b;void(__cdecl*score)(void*,int);void(__cdecl*look)(void*,void*,void*);void(__cdecl*gib)(void*,int);int k,j,p,a,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;score=(void*)(b+0x57230);look=(void*)(b+0x573d0);gib=(void*)(b+0x57460);
 if(!hook(b+0x61b60,ranks)||!hook(CalculateRanks,ranks)||!hook(b+0x950e0,gibEvent)||!hook(G_AddEvent,gibEvent))return 2;
 for(k=0;k<32768;++k){int expectedCalls,value=(k%17)-8;memset(&client,0,sizeof(client));memset(rawClient,0,sizeof(rawClient));ent.client=k%3?&client:NULL;*(void**)(raw+0x190)=k%3?rawClient:NULL;client.ps.persistant[PERS_SCORE]=*(int*)(rawClient+0x110)=37;client.ps.persistant[PERS_TEAM]=*(int*)(rawClient+0x11c)=(k/3)%4;client.sess.game_points=*(int*)(rawClient+0xbf0)=-17;client.ps.stats[STAT_TCE_FLAGS]=*(int*)(rawClient+0xf4)=k&0x400;
  level.warmupTime=*(int*)(b+0x7feb90)=(k/12)%3-1;level.lmsWinningTeam=*(int*)(b+0x221a508)=(k/36)%3-1;g_gametype.integer=*(int*)(b+0x6d8d4c)=(k/108)%10;
  for(j=0;j<4;++j)level.teamScores[j]=*(int*)(b+0x7febf0+j*4)=100+j;calls=0;score(k&0x8000?NULL:raw,value);expectedCalls=calls;calls=0;AddKillScore(k&0x8000?NULL:&ent,value);
  if(calls!=expectedCalls||client.ps.persistant[PERS_SCORE]!=*(int*)(rawClient+0x110)||client.sess.game_points!=*(int*)(rawClient+0xbf0)||memcmp(level.teamScores,b+0x7febf0,16)){fprintf(stderr,"score case%d mismatch\n",k);return 1;}++n;
 }
 calls=0;score(NULL,7);AddKillScore(NULL,7);if(calls)return 3;printf("%d whole AddKillScore comparisons plus NULL early return passed\n",n);n=0;
 ent.client=&client;*(void**)(raw+0x190)=rawClient;
 for(p=0;p<3;++p)for(a=0;a<3;++a)for(k=0;k<4096;++k){gentity_t*pi=p==0?NULL:p==1?&ent:&inf,*pa=a==0?NULL:a==1?&ent:&att;void*ri=p==0?NULL:p==1?raw:rawInf,*ra=a==0?NULL:a==1?raw:rawAtt;
  VectorSet(ent.s.pos.trBase,k%37-18.f,k%41-20.f,k%43-21.f);VectorSet(inf.s.pos.trBase,k%31-15.f,k%29-14.f,k%23-11.f);VectorSet(att.s.pos.trBase,k%17-8.f,k%19-9.f,k%13-6.f);memcpy(raw+0x18,ent.s.pos.trBase,12);memcpy(rawInf+0x18,inf.s.pos.trBase,12);memcpy(rawAtt+0x18,att.s.pos.trBase,12);ent.s.angles[YAW]=*(float*)(raw+0x78)=k*.25f-512.875f;look(raw,ri,ra);LookAtKiller(&ent,pi,pa);if(client.ps.stats[STAT_DEAD_YAW]!=*(int*)(rawClient+0xd8)){fprintf(stderr,"look case%d p%d a%d yaw%d/%d\n",k,p,a,client.ps.stats[STAT_DEAD_YAW],*(int*)(rawClient+0xd8));return 1;}++n;
 }
 printf("%d whole LookAtKiller comparisons passed\n",n);
 for(k=0;k<8192;++k){int expected;byte*otherRaw=b+0x226e2a0+7*0x57c;gentity_t*other=&g_entities[7];memset(otherRaw,0,0x57c);memset(other,0,sizeof(*other));other->inuse=*(int*)(otherRaw+0x194)=k&1;other->client=k&2?&client:NULL;*(void**)(otherRaw+0x190)=k&2?rawClient:NULL;VectorSet(ent.r.currentOrigin,k%31-15.f,k%29-14.f,k%23-11.f);VectorSet(other->r.currentOrigin,k%17-8.f,k%19-9.f,k%13-6.f);VectorSet(other->s.pos.trDelta,k&4?0:k%11-5.f,k&4?0:k%7-3.f,k&4?0:k%5-2.f);memcpy(raw+0x168,ent.r.currentOrigin,12);memcpy(otherRaw+0x168,other->r.currentOrigin,12);memcpy(otherRaw+0x24,other->s.pos.trDelta,12);ent.takedamage=*(int*)(raw+0x2cc)=1;ent.s.eType=*(int*)(raw+4)=1;ent.r.contents=*(int*)(raw+0x14c)=123;
  mode=1;bad=calls=0;gib(raw,7);expected=parmSeen;if(bad||calls!=1)return 4;mode=0;bad=calls=0;GibEntity(&ent,7);if(bad||calls!=1||parmSeen!=expected||ent.takedamage!=*(int*)(raw+0x2cc)||ent.s.eType!=*(int*)(raw+4)||ent.r.contents!=*(int*)(raw+0x14c)){fprintf(stderr,"gib case%d mismatch dir%d/%d\n",k,parmSeen,expected);return 1;}
 }
 puts("8192 whole GibEntity comparisons passed");return 0;
}
