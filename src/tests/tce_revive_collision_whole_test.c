#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern qboolean StuckInClient(gentity_t*);extern void WolfReviveBbox(gentity_t*),WolfRevivePushEnt(gentity_t*,gentity_t*);
static byte*base,clientsRaw[10][0x16b0];static gclient_t clients[10];static int side,v,count,bad;
typedef struct{int k,a;float bounds[6];char text[96];}event;static event calls[40];
typedef struct{int ret,contents,reviver,count;float origin[6];event calls[40];}result;
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static byte*raw(int n){return base+0x226e2a0+n*0x57c;}static void*ent(int n){return side?(void*)&g_entities[n]:raw(n);}static int idx(void*p){return side?(gentity_t*)p-g_entities:((byte*)p-raw(0))/0x57c;}
static event*note(int k,int a){event*e=&calls[count++];if(count>40)exit(3);e->k=k;e->a=a;return e;}
static void*position(void*p){note(1,idx(p));return v%6?ent(8):NULL;}
static int box(const float*a,const float*b,int*out,int max){event*e=note(2,max);int i,n=(v/6)%10;memcpy(e->bounds,a,12);memcpy(e->bounds+3,b,12);for(i=0;i<n;i++)out[i]=i;return n;}
static int contact(const float*a,const float*b,void*p){event*e=note(3,idx(p));memcpy(e->bounds,a,12);memcpy(e->bounds+3,b,12);return (v/60)%3==0?0:(v/60)%3==1?1:idx(p)%2;}
static void push(void*a,void*b){note(4,idx(a));if(idx(b)!=0)bad=1;}
static void link(void*p){note(5,idx(p));}
static void debug(const char*fmt,...){va_list a;event*e=note(6,0);va_start(a,fmt);Q_vsnprintf(e->text,sizeof(e->text),fmt,a);va_end(a);}
static void capture(result*r,int ret){memset(r,0,sizeof(*r));r->ret=ret;r->count=count;r->contents=side?g_entities[0].r.contents:*(int*)(raw(0)+0x14c);r->reviver=side?g_entities[0].props_frame_state:*(int*)(raw(0)+0x378);memcpy(r->origin,side?(void*)g_entities[0].r.currentOrigin:raw(0)+0x168,12);memcpy(r->origin+3,side?(void*)clients[0].ps.origin:clientsRaw[0]+0x14,12);memcpy(r->calls,calls,sizeof(calls));}
int main(int argc,char**argv){HMODULE dll;int(__cdecl*stuck)(void*);void(__cdecl*bbox)(void*);int phase,i,j,total=0,ret;static result a,b;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;stuck=(void*)(base+0x46fa0);bbox=(void*)(base+0x47280);
#define H(r,f,t) if(!hook(base+r,t)||!hook(f,t))return 2
 H(0x6dc00,G_TestEntityPosition,position);H(0x8afe0,trap_EntitiesInBox,box);H(0x8b000,trap_EntityContactCapsule,contact);H(0x47150,WolfRevivePushEnt,push);H(0x8afa0,trap_LinkEntity,link);H(0x5edc0,G_DPrintf,debug);
 for(phase=0;phase<2;phase++)for(v=0;v<16384;v++)for(side=0;side<2;side++){
  count=bad=0;memset(calls,0,sizeof(calls));memset(clients,0,sizeof(clients));memset(clientsRaw,0,sizeof(clientsRaw));level.numConnectedClients=*(int*)(base+0x7fec0c)=v%10;g_newbbox.integer=*(int*)(base+0x7e208c)=(v/180)%3-1;
  for(i=0;i<10;i++){gentity_t*e=&g_entities[i];byte*p=raw(i);memset(e,0,sizeof(*e));memset(p,0,0x57c);level.sortedClients[i]=*(int*)(base+0x7fec18+i*4)=9-i;e->client=&clients[i];*(void**)(p+0x190)=clientsRaw[i];
#define E(f,o,x) e->f=*(int*)(p+o)=(x)
   E(s.number,0,i);E(inuse,0x194,((v/10+i)%3)!=0);E(s.solid,0xb4,((v/30+i)%3)!=0);E(health,0x2c8,(v/90+i)%3-1);E(r.contents,0x14c,((v/270+i)%4)==0?1:((v/270+i)%4)==1?0x2000000:((v/270+i)%4)==2?0x10000:0);E(props_frame_state,0x378,(v/810)%2?7:-1);
   if(i>0&&i!=7&&(v/2430+i)%3==0){e->client=NULL;*(void**)(p+0x190)=NULL;}
   for(j=0;j<3;j++){float z=(float)((v/3+i*7+j)%31)-15;e->r.currentOrigin[j]=*(float*)(p+0x168+4*j)=z;e->r.mins[j]=*(float*)(p+0x134+4*j)=-14;e->r.maxs[j]=*(float*)(p+0x140+4*j)=14;clients[i].ps.origin[j]=*(float*)(clientsRaw[i]+0x14+4*j)=i*100.f+j;}
  }
  /* Precision boundary: only self.maxX and hit.minX stay extended in Windows. */
  if(!phase&&(v&1)){g_entities[1].inuse=*(int*)(raw(1)+0x194)=1;g_entities[1].s.solid=*(int*)(raw(1)+0xb4)=1;g_entities[1].health=*(int*)(raw(1)+0x2c8)=1;g_entities[1].client=&clients[1];*(void**)(raw(1)+0x190)=clientsRaw[1];for(i=0;i<2;i++)for(j=0;j<3;j++)g_entities[i].r.currentOrigin[j]=*(float*)(raw(i)+0x168+j*4)=j?0:16777216.f;g_entities[0].r.maxs[0]=*(float*)(raw(0)+0x140)=.25f;g_entities[1].r.mins[0]=*(float*)(raw(1)+0x134)=.5f;level.numConnectedClients=*(int*)(base+0x7fec0c)=1;level.sortedClients[0]=*(int*)(base+0x7fec18)=1;}
  if(phase){g_entities[8].client=&clients[8];*(void**)(raw(8)+0x190)=clientsRaw[8];g_entities[8].s.number=*(int*)raw(8)=v%6==1?ENTITYNUM_WORLD:8;clients[8].ps.persistant[PERS_HWEAPON_USE]=*(int*)(clientsRaw[8]+0x140)=v%6==2;clients[8].ps.eFlags=*(int*)(clientsRaw[8]+0x68)=v%6==3?0x8000:0;if(v%6==4){g_entities[8].client=NULL;*(void**)(raw(8)+0x190)=NULL;}if(side)WolfReviveBbox(ent(0));else bbox(ent(0));ret=0;}else ret=side?StuckInClient(ent(0)):stuck(ent(0));capture(side?&b:&a,ret);
  if(side){if(bad||memcmp(&a,&b,sizeof(a))){printf("Revive collision mismatch phase%d case%d bad%d return%d/%d calls%d/%d\n",phase,v,bad,a.ret,b.ret,a.count,b.count);for(i=0;i<sizeof(a);i++)if(((byte*)&a)[i]!=((byte*)&b)[i]){printf("byte%x %02x/%02x\n",i,((byte*)&a)[i],((byte*)&b)[i]);break;}return 1;}total++;}
 }
 printf("PASS whole StuckInClient and WolfReviveBbox %d cases (16384 each)\n",total);return 0;
}

