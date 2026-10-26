#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern void Touch_flagonly_multiple(gentity_t*,gentity_t*,trace_t*);
extern int Team_TouchOurFlag(gentity_t*,gentity_t*,int);
extern int Team_TouchEnemyFlag(gentity_t*,gentity_t*,int);
extern int Pickup_Team(gentity_t*,gentity_t*);
extern void Team_ResetFlag(gentity_t*);
extern void QDECL PrintMsg(gentity_t*,const char*,...);
static byte raw[0x57c],ro[0x57c],rc[0x2000],*base;static gentity_t ent,other;static gclient_t cl;
static int side,count,bad,mutation;
typedef struct {int kind,a,b;char text[128];} rec;static rec logs[32];
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static int id(void*p){int i;if(!p)return -1;if(p==(side?(void*)&ent:(void*)raw))return 100;if(p==(side?(void*)&other:(void*)ro))return 101;for(i=20;i<24;i++)if(p==(side?(void*)&g_entities[i]:(void*)(base+0x226e2a0+i*0x57c)))return i;bad=1;return -2;}
static void note(int k,int a,int b,const char*s){rec*r;if(count>=32)exit(3);r=&logs[count++];memset(r,0,sizeof(*r));r->kind=k;r->a=a;r->b=b;if(s)Q_strncpyz(r->text,s,sizeof(r->text));}
static void score(void*p,int v){note(1,id(p),v,NULL);}
static void kill(void*p,int v){note(2,id(p),v,NULL);}
static void script(void*p,const char*s,const char*v){int n=id(p),parent=n==100?id(side?(void*)ent.parent:*(void**)(raw+0x218)):-1;note(3,n,parent,s);note(4,0,0,v);if(mutation&&n==100){if(side){cl.flagParent=22;ent.spawnflags^=4;}else{*(int*)(rc+0x1228)=22;*(int*)(raw+0x1a8)^=4;}}}
static void reset(void*p){note(5,id(p),0,NULL);}
static void respawn(void*p){note(6,id(p),0,NULL);}
static void print(const char*s,...){note(7,0,0,s);}
static void bot(int team,const char*s,const char*v){note(8,team,0,s);note(9,0,0,v);}
static void msg(void*p,const char*s,...){note(10,id(p),0,s);}
int main(int argc,char**argv){HMODULE dll;int i,j,mode,n=0;const char*names[]={"team_CTF_redflag","team_CTF_blueflag","TEAM_CTF_redflag","invalid"};float scores[]={20,10.9f,-3.75f,0};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;
 if(!hook(base+0x571d0,score)||!hook(AddScore,score)||!hook(base+0x57230,kill)||!hook(AddKillScore,kill)||!hook(base+0x7d4b0,script)||!hook(G_Script_ScriptEvent,script)||!hook(base+0x8dae0,reset)||!hook(Team_ResetFlag,reset)||!hook(base+0x5dbf0,respawn)||!hook(RespawnItem,respawn)||!hook(base+0x5ed80,print)||!hook(G_Printf,print)||!hook(base+0x21920,bot)||!hook(Bot_TeamScriptEvent,bot)||!hook(base+0x8d9f0,msg)||!hook(PrintMsg,msg))return 2;
 for(mode=0;mode<4;mode++)for(i=0;i<16384;i++){
  rec expected[32];int ec,x=0,y=0;byte before[0x2000];
  memset(raw,0,sizeof(raw));memset(ro,0,sizeof(ro));memset(rc,0,sizeof(rc));memset(&ent,0,sizeof(ent));memset(&other,0,sizeof(other));memset(&cl,0,sizeof(cl));
  other.client=&cl;*(void**)(ro+0x190)=rc;if(mode==0&&(i&8192)){other.client=NULL;*(void**)(ro+0x190)=NULL;}
  ent.parent=&g_entities[20];*(void**)(raw+0x218)=base+0x226e2a0+20*0x57c;
  ent.spawnflags=*(int*)(raw+0x1a8)=i&7;ent.entstate=*(int*)(raw+0x47c)=(i>>3)%7;
  cl.ps.powerups[PW_REDFLAG]=*(int*)(rc+0x168)=(i&64)?INT_MAX:0;cl.ps.powerups[PW_BLUEFLAG]=*(int*)(rc+0x16c)=(i&128)?INT_MAX:0;cl.ps.powerups[PW_OPS_DISGUISED]=*(int*)(rc+0x170)=333;
  cl.speedScale=*(int*)(rc+0x102c)=71;cl.flagParent=*(int*)(rc+0x1228)=21;ent.accuracy=*(float*)(raw+0x420)=scores[(i>>8)&3];
  cl.sess.sessionTeam=*(int*)(rc+0xb30)=(i>>10)&3;ent.flags=*(int*)(raw+0x1b0)=(i&4096)?FL_DROPPED_ITEM:0;
  ent.s.number=*(int*)(raw)=21;ent.s.otherEntityNum=*(int*)(raw+0x8c)=22;ent.s.density=*(int*)(raw+0xf4)=(i>>4)%5-2;
  ent.splashDamage=*(int*)(raw+0x2d4)=42;ent.classname=names[(i>>8)&3];*(void**)(raw+0x1a4)=ent.classname;ent.message="flag";*(void**)(raw+0x248)=ent.message;ent.s.modelindex2=*(int*)(raw+0xa8)=129;
  level.time=*(int*)(base+0x7febe0)=16777217;g_gametype.integer=*(int*)(base+0x6d8d4c)=(i>>11)&7;
  level.gameManager=(i&512)?&g_entities[23]:NULL;*(void**)(base+0x221b71c)=(i&512)?base+0x226e2a0+23*0x57c:NULL;
  for(j=20;j<24;j++){memset(&g_entities[j],0,sizeof(gentity_t));memset(base+0x226e2a0+j*0x57c,0,0x57c);g_entities[j].s.density=*(int*)(base+0x226e2a0+j*0x57c+0xf4)=(i>>6)%4-1;}
  mutation=(i>>12)&1;side=0;count=bad=0;
  if(mode==0)((void(__cdecl*)(void*,void*,void*))(base+0x93260))(raw,ro,NULL);
  else if(mode==1)x=((int(__cdecl*)(void*,void*,int))(base+0x8dc00))(raw,ro,(i>>3)&3);
  else if(mode==2)x=((int(__cdecl*)(void*,void*,int))(base+0x8dce0))(raw,ro,(i>>3)&3);
  else x=((int(__cdecl*)(void*,void*))(base+0x8de30))(raw,ro);
  ec=count;memcpy(expected,logs,sizeof(expected));side=1;count=0;
  if(mode==0)Touch_flagonly_multiple(&ent,&other,NULL);else if(mode==1)y=Team_TouchOurFlag(&ent,&other,(i>>3)&3);else if(mode==2)y=Team_TouchEnemyFlag(&ent,&other,(i>>3)&3);else y=Pickup_Team(&ent,&other);
  if(bad||ec!=count||x!=y||memcmp(expected,logs,count*sizeof(rec))){fprintf(stderr,"flag controller mode%d case%d calls%d/%d return%d/%d bad%d\n",mode,i,count,ec,y,x,bad);return 1;}
  if(ent.s.density!=*(int*)(raw+0xf4)||cl.ps.powerups[PW_REDFLAG]!=*(int*)(rc+0x168)||cl.ps.powerups[PW_BLUEFLAG]!=*(int*)(rc+0x16c)||cl.ps.powerups[PW_OPS_DISGUISED]!=*(int*)(rc+0x170)||cl.flagParent!=*(int*)(rc+0x1228)||cl.speedScale!=*(int*)(rc+0x102c)||cl.pers.teamState.flagsince!=*(float*)(rc+0x658)||other.message!=*(char**)(ro+0x248)||other.s.otherEntityNum2!=*(int*)(ro+0x90)){fprintf(stderr,"flag controller mode%d case%d state mismatch\n",mode,i);return 1;}
  side=1;if(id(ent.parent)!=20)return 1;for(j=20;j<24;j++)if(g_entities[j].s.density!=*(int*)(base+0x226e2a0+j*0x57c+0xf4))return 1;n++;
 }
 printf("%d original flag pickup/return/capture controller comparisons passed\n",n);return 0;
}
