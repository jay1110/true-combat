#define WIN32_LEAN_AND_MEAN
#include "../game/g_stats.c"
#include <windows.h>
#include <stdio.h>
/* Exercise the actual production functions; only external side effects and
 * the separately reconstructed upgrade controller are intercepted. */
static char events[4096];
static void patch(void *a,void *f){DWORD o,t;unsigned char *p=a;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void print(const char *fmt,...){char s[1024];va_list ap;va_start(ap,fmt);vsprintf(s,fmt,ap);va_end(ap);strcat(events,s);}
static void pb(int n,const char *kind,const char *s){strcat(events,va("pb:%d:%s:%s;",n,kind,s));}
static void upgrade(void *e,int skill){strcat(events,va("upgrade:%d;",skill));}
static gclient_t client;
static unsigned char rc[0x16b0];
static const float values[]={-1,0,.5f,19.5f,20,49.5f,50,89.5f,90,139.5f,140,200};
static void seed(unsigned char *b,int n,int skill,int team,int absent){int i,j;
 memset(&client,0,sizeof(client));memset(rc,0,sizeof(rc));
 memset(g_entities,0,sizeof(g_entities[0]));memset(b+0x226e2a0,0,0x57c);
 g_entities[0].client=absent?NULL:&client;*(void **)(b+0x226e2a0+400)=absent?NULL:rc;
 client.sess.sessionTeam=*(int *)(rc+0xb30)=team;client.ps.persistant[PERS_TEAM]=*(int *)(rc+0x11c)=team;
 client.sess.playerType=*(int *)(rc+0xb40)=n%7;strcpy(client.pers.netname,"rating-test");strcpy((char *)rc+0x5f8,"rating-test");
 for(i=0;i<7;++i){client.sess.skillpoints[i]=*(float *)(rc+0xb68+4*i)=values[(n+i)%12];client.sess.skill[i]=*(int *)(rc+0xba4+4*i)=n%5;
  for(j=0;j<2;++j)level.teamXP[i][j]=*(float *)(b+0x222284c+8*i+4*j)=17.5f+i+j;
 }
 for(i=0;i<4;++i)level.teamScores[i]=*(int *)(b+0x7febf0+4*i)=123+i;
 client.ps.persistant[PERS_SCORE]=*(int *)(rc+0x110)=37;
}
static int same(unsigned char *b){int i,j;
 if(client.ps.persistant[PERS_SCORE]!=*(int *)(rc+0x110))return 0;
 for(i=0;i<7;++i){if(memcmp(&client.sess.skillpoints[i],rc+0xb68+4*i,4)||client.sess.skill[i]!=*(int *)(rc+0xba4+4*i))return 0;
  for(j=0;j<2;++j)if(level.teamXP[i][j]!=*(float *)(b+0x222284c+8*i+4*j))return 0;
 }
 for(i=0;i<4;++i)if(level.teamScores[i]!=*(int *)(b+0x7febf0+4*i))return 0;
 return 1;
}
int main(int argc,char **argv){HMODULE h;unsigned char *b;int op,gt,state,team,n,skill,absent,count=0;char expected[4096];
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
 patch(b+0x8bad0,pb);patch(trap_PbStat,pb);patch(b+0x5ed80,print);patch(G_Printf,print);patch(b+0x86e60,upgrade);patch(G_UpgradeSkill,upgrade);
 for(n=0;n<12;++n)for(skill=0;skill<7;++skill)for(op=0;op<2;++op){seed(b,n,skill,1,0);
  if(op){((void(__cdecl *)(void *,int))(b+0x86b20))(rc,skill);G_SetPlayerSkill(&client,skill);}
  else{((void(__cdecl *)(void *))(b+0x86ae0))(rc);G_SetPlayerScore(&client);}
  if(!same(b)){printf("score/skill mismatch %d %d %d\n",op,n,skill);return 1;}++count;
 }
 for(op=0;op<2;++op)for(gt=0;gt<9;++gt)for(state=0;state<3;++state)for(team=0;team<4;++team)for(n=0;n<12;++n)for(skill=0;skill<7;++skill)for(absent=0;absent<2;++absent){
  float points=values[n];seed(b,n,skill,team,absent);g_gametype.integer=*(int *)(b+0x6d8d4c)=gt;g_gamestate.integer=*(int *)(b+0x784f6c)=state;
  events[0]=0;((void(__cdecl *)(void *,int,float))(b+(op?0x86d20:0x86b70)))(b+0x226e2a0,skill,points);strcpy(expected,events);events[0]=0;
  if(op)G_AddSkillPoints(g_entities,skill,points);else G_LoseSkillPoints(g_entities,skill,points);
  if(!same(b)||strcmp(expected,events)){printf("skill points mismatch op%d gt%d state%d team%d n%d skill%d absent%d\n%s\n%s\n",op,gt,state,team,n,skill,absent,expected,events);return 1;}++count;
 }
 printf("%d complete original score/skill/add/loss comparisons passed\n",count);return 0;
}
