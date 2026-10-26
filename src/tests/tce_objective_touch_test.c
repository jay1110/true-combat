#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern void SP_func_obj_touch(gentity_t*),SP_func_obj_use(gentity_t*),InitTrigger(gentity_t*);
static byte *base,r[0x57c],o[0x57c],p[0x57c],c[0x2000],clients[4][0x16b0];
static gentity_t ent,other,parent;static gclient_t client,sc[4];static int side,count,bad,variant,boxes;
typedef struct{int k,a,b;float bounds[6];char s[64];}record;static record logs[32];
static void note(int k,int a,int b,const char*s){record*q;if(count>=32)exit(3);q=&logs[count++];memset(q,0,sizeof(*q));q->k=k;q->a=a;q->b=b;if(s)Q_strncpyz(q->s,s,sizeof(q->s));}
static int hook(void*a,void*f){DWORD v,w;byte*q=a;if(!VirtualProtect(a,5,PAGE_EXECUTE_READWRITE,&v))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),a,5);VirtualProtect(a,5,v,&w);return 1;}
static void dummy(void){}
static void assertent(void*x){if(x!=(side?(void*)&ent:(void*)r))bad=1;}
static void init(void*x){assertent(x);note(1,0,0,NULL);if(side){ent.s.origin[0]+=3;ent.s.eType=99;}else{*(float*)(r+0x5c)+=3;*(int*)(r+4)=99;}}
static void mover(void*x){assertent(x);note(2,0,0,NULL);if(side){ent.s.origin[1]-=7;ent.spawnflags^=variant&16;}else{*(float*)(r+0x60)-=7;*(int*)(r+0x1a8)^=variant&16;}}
static int model(const char*x){note(3,0,0,x);return 417;}
static void brush(void*x,const char*s){assertent(x);note(4,0,0,s);}
static void link(void*x){assertent(x);note(5,side?ent.s.eType:*(int*)(r+4),0,NULL);if(side){ent.s.origin[2]+=11;ent.spawnflags^=variant&16;}else{*(float*)(r+0x64)+=11;*(int*)(r+0x1a8)^=variant&16;}}
static int box(const float*lo,const float*hi,int*list,int max){int j,n=(variant+boxes)%4;note(6,max,n,NULL);memcpy(logs[count-1].bounds,lo,12);memcpy(logs[count-1].bounds+3,hi,12);boxes++;for(j=0;j<n;j++)list[j]=30+j;return n;}
static void script(void*x,const char*s,const char*v){assertent(x);note(7,0,0,s);note(8,0,0,v);if(variant&2){if(side)client.sess.sessionTeam=TEAM_ALLIES;else*(int*)(c+0xb30)=2;}}
static void use(void*x,void*y){assertent(x);if(y!=(side?(void*)&other:(void*)o))bad=1;note(9,0,0,NULL);if(side)level.time+=variant&2;else*(int*)(base+0x7febe0)+=variant&2;}
int main(int argc,char**argv){HMODULE dll;void(*touch)(gentity_t*,gentity_t*,trace_t*);int mode,f,inv,team,v,j,n=0;record expected[32];int ec;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;
 if(!hook(base+0x91e90,init)||!hook(InitTrigger,init)||!hook(base+0x700a0,mover)||!hook(InitMover,mover)||!hook(base+0x947d0,model)||!hook(G_ModelIndex,model)||!hook(base+0x8ae60,brush)||!hook(trap_SetBrushModel,brush)||!hook(base+0x8afa0,link)||!hook(trap_LinkEntity,link)||!hook(base+0x8afe0,box)||!hook(trap_EntitiesInBox,box)||!hook(base+0x7d4b0,script)||!hook(G_Script_ScriptEvent,script)||!hook(base+0x94b90,use)||!hook(G_UseTargets,use))return 2;
 side=1;SP_func_obj_touch(&ent);touch=ent.touch;
 for(mode=0;mode<2;mode++)for(f=0;f<256;f++)for(v=0;v<8;v++){
  memset(&ent,0,sizeof(ent));memset(r,0,sizeof(r));variant=v*8;ent.spawnflags=*(int*)(r+0x1a8)=f;ent.model="*19";*(char**)(r+0x1b4)=ent.model;ent.model2=v&1?"model":NULL;*(char**)(r+0x1b8)=ent.model2;
  ent.touch=(void*)dummy;*(void**)(r+0x2b0)=dummy;ent.use=(void*)dummy;*(void**)(r+0x2b4)=dummy;ent.reached=(void*)dummy;*(void**)(r+0x2a8)=dummy;
  for(j=0;j<3;j++)ent.s.origin[j]=*(float*)(r+0x5c+j*4)=(float)(v*17-j*23);
  side=0;count=bad=0;((void(__cdecl*)(void*))(base+(mode?0x75a60:0x759c0)))(r);ec=count;memcpy(expected,logs,sizeof(expected));
  side=1;count=0;if(mode)SP_func_obj_use(&ent);else SP_func_obj_touch(&ent);
  if(bad||ec!=count||memcmp(expected,logs,count*sizeof(record))||ent.s.eType!=*(int*)(r+4)||ent.s.modelindex2!=*(int*)(r+0xa8)||memcmp(ent.s.pos.trBase,r+0x18,12)||memcmp(ent.r.currentOrigin,r+0x168,12)||ent.use||ent.reached||((ent.touch==touch)!=(*(void**)(r+0x2b0)==base+0x75860))){fprintf(stderr,"objective spawn mode%d flags%d v%d failed\n",mode,f,v);return 1;}n++;
 }
 for(f=0;f<128;f++)for(inv=0;inv<8;inv++)for(team=0;team<4;team++)for(v=0;v<64;v++){
  memset(r,0,sizeof(r));memset(o,0,sizeof(o));memset(c,0,sizeof(c));memset(p,0,sizeof(p));memset(&ent,0,sizeof(ent));memset(&other,0,sizeof(other));memset(&client,0,sizeof(client));memset(&parent,0,sizeof(parent));
  variant=v;ent.spawnflags=*(int*)(r+0x1a8)=f;ent.entstate=*(int*)(r+0x47c)=v&7;ent.eventTime=*(int*)(r+0x1c0)=10000+((v&8)?1:0);level.time=*(int*)(base+0x7febe0)=10000;
  other.client=&client;*(void**)(o+0x190)=c;client.sess.sessionTeam=*(int*)(c+0xb30)=team;client.ps.stats[STAT_TCE_WEAPON_FLAGS]=*(int*)(c+0xf0)=(inv&1)?0x100:0;client.ps.stats[STAT_TCE_FLAGS]=*(int*)(c+0xf4)=((inv&2)?0x400:0)|((inv&4)?0x100:0);
  ent.parent=(v&16)?NULL:&parent;*(void**)(r+0x218)=(v&16)?NULL:p;parent.s.eType=*(int*)(p+4)=(v&32)?ET_GENERAL:ET_OID_TRIGGER;ent.s.teamNum=*(int*)(r+0x108)=2;
  memset(sc,0,sizeof(sc));memset(clients,0,sizeof(clients));level.clients=sc;*(void**)(base+0x7feb80)=clients;level.numConnectedClients=*(int*)(base+0x7fec0c)=v%5;
  for(j=0;j<4;j++){int ix=3-j;level.sortedClients[j]=*(int*)(base+0x7fec18+4*j)=ix;sc[ix].sess.sessionTeam=*(int*)(clients[ix]+0xb30)=(v+j)%4;sc[ix].pers.connected=*(int*)(clients[ix]+0x5ac)=(v+j)%3;sc[ix].ps.stats[STAT_TCE_FLAGS]=*(int*)(clients[ix]+0xf4)=(j&1)?0x400:0;sc[ix].ps.origin[0]=*(float*)(clients[ix]+0x14)=j*53.5f;}
  for(j=0;j<3;j++){g_entities[30+j].s.eType=*(int*)(base+0x226e2a0+(30+j)*0x57c+4)=((v+j)&1)?63:65;g_entities[30+j].s.teamNum=*(int*)(base+0x226e2a0+(30+j)*0x57c+0x108)=j;}
  side=0;count=bad=boxes=0;((void(__cdecl*)(void*,void*,void*))(base+0x75860))(r,o,NULL);ec=count;memcpy(expected,logs,sizeof(expected));
  side=1;count=boxes=0;touch(&ent,&other,NULL);
  if(bad||ec!=count||memcmp(expected,logs,count*sizeof(record))||ent.eventTime!=*(int*)(r+0x1c0)||level.time!=*(int*)(base+0x7febe0)||client.sess.sessionTeam!=*(int*)(c+0xb30)){fprintf(stderr,"objective touch flags%d inv%d team%d v%d calls%d/%d failed\n",f,inv,team,v,count,ec);return 1;}n++;
 }
 printf("%d objective spawn/use/touch original comparisons passed (G_AllInTrigger dependency real)\n",n);return 0;
}
