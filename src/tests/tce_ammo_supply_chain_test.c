#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern void ammo_touch(gentity_t*,gentity_t*,trace_t*),SP_trigger_ammo(gentity_t*),SP_misc_cabinet_supply(gentity_t*),InitTrigger(gentity_t*),trigger_ammo_think(gentity_t*),trigger_ammo_setup(gentity_t*);
extern qboolean G_IsAllowedAmmo(gentity_t*);
static byte*base;static int side,variant,count,bad;static gclient_t dummy;
typedef struct{int k,a,b;char text[64];}event;static event events[256];
typedef struct{int count,health,capacity,damage,timestamp,next,touch,think,target,contents,clipmask;entityState_t state;float vectors[12];event events[256];}result;
static const char*totals[]={"0","-1","1","3"," 27","16777217"};static const char*rates[]={"1","2","-3","0","10"," 7"};
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static void*entity(int n){return side?(void*)&g_entities[n]:(void*)(base+0x226e2a0+n*0x57c);}
static int idx(void*p){return side?(gentity_t*)p-g_entities:((byte*)p-base-0x226e2a0)/0x57c;}
static void note(int k,int a,int b,const char*s){event*e;if(count==256)exit(3);e=&events[count++];memset(e,0,sizeof(*e));e->k=k;e->a=a;e->b=b;if(s)Q_strncpyz(e->text,s,sizeof(e->text));}
static int contact(const float*mi,const float*ma,void*p){int i=(int)mi[0];if(p!=entity(70)||ma[0]!=i+1)bad=1;note(1,i,0,NULL);return (variant/4)%3==0?1:(variant/4)%3==1?(i%2):0;}
static int allowed(void*p){int n=idx(p);note(2,n,0,NULL);return (variant/12)%3==0?1:(variant/12)%3==1?(n%2):0;}
static int addAmmo(void*p,int amount){int n=idx(p);note(3,n,amount,NULL);return (variant/36)%3==0?1:(variant/36)%3==1?(n%2):0;}
static void*item(const char*s){note(4,0,0,s);return side?(void*)&bg_itemlist[7]:(void*)(base+0xb6b38+7*0x3c);}
static void predictable(void*p,int ev,int parm){note(5,idx(p),parm,NULL);if(ev!=(side?EV_ITEM_PICKUP:29))bad=1;}
static void init(void*p){note(6,idx(p),0,NULL);}
static int spawnString(const char*k,const char*d,char**out,const char*file,int line){note(7,0,0,k);Q_strncpyz(events[count-1].text+32,d,32);if(!file||line<1)bad=1;*out=(char*)(!strcmp(k,"ammototal")?totals[variant%6]:rates[(variant/6)%6]);return 1;}
static void link(void*p){note(8,idx(p),0,NULL);}
static void capture(result*r){gentity_t*e=&g_entities[70];byte*p=base+0x226e2a0+70*0x57c;void*f;memset(r,0,sizeof(*r));r->count=count;memcpy(r->events,events,sizeof(events));memcpy(&r->state,side?(void*)&e->s:(void*)p,sizeof(entityState_t));
#define V(name,field,off) r->name=side?e->field:*(int*)(p+off)
 V(health,health,0x2c8);V(capacity,count,0x2e4);V(damage,damage,0x2d0);V(timestamp,timestamp,0x24c);V(next,nextthink,0x29c);V(contents,r.contents,0x14c);V(clipmask,clipmask,0x1d4);r->target=side?(e->target_ent!=NULL):(*(void**)(p+0x264)!=NULL);r->touch=side?(e->touch==ammo_touch):(*(void**)(p+0x2b0)==base+0x92a90);f=side?(void*)e->think:*(void**)(p+0x2a4);r->think=!f?0:f==(side?(void*)trigger_ammo_think:(void*)(base+0x92c30))?1:f==(side?(void*)trigger_ammo_setup:(void*)(base+0x92c70))?2:3;memcpy(r->vectors,side?(void*)e->r.mins:(void*)(p+0x134),24);memcpy(r->vectors+6,side?(void*)e->r.currentOrigin:(void*)(p+0x168),24);
}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*touch)(void*,void*,void*);void(__cdecl*spawn)(void*),(__cdecl*cabinet)(void*);int phase,v,i,j,total=0;result a,b;int health[]={-9999,-1,0,1,3,10,16777217,2147483647},damage[]={1,2,3,7,-1};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;touch=(void*)(base+0x92a90);spawn=(void*)(base+0x92d50);cabinet=(void*)(base+0x92cd0);if(sizeof(entityState_t)!=0x120)return 2;
#define H(r,fn,repl) if(!hook(base+r,repl)||!hook(fn,repl))return 2
 H(0x8b000,trap_EntityContactCapsule,contact);H(0x92a50,G_IsAllowedAmmo,allowed);H(0x5c900,AddMagicAmmo,addAmmo);H(0x2b240,BG_FindItem,item);H(0x950b0,G_AddPredictableEvent,predictable);H(0x91e90,InitTrigger,init);H(0x85940,G_SpawnStringExt,spawnString);H(0x8afa0,trap_LinkEntity,link);
 for(phase=0;phase<3;phase++)for(v=0;v<4096;v++)for(side=0;side<2;side++){
  variant=v;count=bad=0;memset(events,0,sizeof(events));level.time=*(int*)(base+0x7febe0)=100000;level.numConnectedClients=*(int*)(base+0x7fec0c)=(v/108)%9;
  for(i=0;i<72;i++){gentity_t*e=&g_entities[i];byte*p=base+0x226e2a0+i*0x57c;memset(e,0,sizeof(*e));memset(p,0,0x57c);e->s.number=*(int*)p=i;e->r.absmin[0]=*(float*)(p+0x150)=(float)i;e->r.absmax[0]=*(float*)(p+0x15c)=(float)i+1;if(i<64)level.sortedClients[i]=*(int*)(base+0x7fec18+i*4)=7-i%8;}
  {gentity_t*e=&g_entities[70];byte*p=base+0x226e2a0+70*0x57c;
#define E(field,off,val) e->field=*(int*)(p+off)=(val)
   E(health,0x2c8,health[(v/4)%8]);E(count,0x2e4,777);E(damage,0x2d0,damage[(v/32)%5]);E(timestamp,0x24c,100000+v%3-1);E(nextthink,0x29c,333);e->target=(v/36)%3==0?NULL:(v/36)%3==1?"":"supply";*(void**)(p+0x254)=e->target;e->target_ent=&g_entities[0];*(void**)(p+0x264)=base+0x226e2a0;g_entities[71].client=v%4?&dummy:NULL;*(void**)(p+0x57c+0x190)=v%4?&dummy:NULL;for(j=0;j<3;j++){e->s.origin[j]=*(float*)(p+0x5c+j*4)=(v%9-4)*.25f+j;e->s.angles[j]=*(float*)(p+0x74+j*4)=j*30.f-15;}
  }
  if(phase==0){if(side)ammo_touch(entity(70),entity(71),NULL);else touch(entity(70),entity(71),NULL);}else if(phase==1){if(side)SP_trigger_ammo(entity(70));else spawn(entity(70));}else{if(side)SP_misc_cabinet_supply(entity(70));else cabinet(entity(70));}
  capture(side?&b:&a);if(side){if(bad||memcmp(&a,&b,sizeof(a))){printf("Ammo supply mismatch phase%d case%d bad%d calls%d/%d health%d/%d capacity%d/%d next%d/%d\n",phase,v,bad,a.count,b.count,a.health,b.health,a.capacity,b.capacity,a.next,b.next);for(i=0;i<sizeof(a);i++)if(((byte*)&a)[i]!=((byte*)&b)[i]){printf("resultbyte%x %02x/%02x\n",i,((byte*)&a)[i],((byte*)&b)[i]);break;}for(i=0;i<a.count||i<b.count;i++)if(memcmp(&a.events[i],&b.events[i],sizeof(event)))printf("call%d %d,%d,%d[%s] / %d,%d,%d[%s]\n",i,a.events[i].k,a.events[i].a,a.events[i].b,a.events[i].text,b.events[i].k,b.events[i].a,b.events[i].b,b.events[i].text);return 1;}total++;}
 }
 printf("PASS whole ammo touch, trigger spawn and supply cabinet %d cases (4096 each)\n",total);return 0;
}

