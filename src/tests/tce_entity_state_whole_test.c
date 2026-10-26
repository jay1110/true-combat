#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern void Team_DroppedFlagThink(gentity_t*);
static byte*base;static int side,variant,bad,count;static mapEntityData_t marker[2];static byte rawMarker[2][128];static gitem_t item;static byte rawItem[64];
typedef struct {int k,a,b;char text[128];} event;static event events[128];
static void*entity(int n){return side?(void*)&g_entities[n]:(void*)(base+0x226e2a0+n*0x57c);}
static int id(void*p){int n;if(!p)return -1;for(n=10;n<18;n++)if(p==entity(n))return n;bad=1;return -2;}
static void note(int k,int a,int b,const char*s){event*e;if(count>=128)exit(3);e=&events[count++];memset(e,0,sizeof(*e));e->k=k;e->a=a;e->b=b;if(s)Q_strncpyz(e->text,s,sizeof(e->text));}
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static void link(void*p){note(1,id(p),0,NULL);}static void unlinker(void*p){note(2,id(p),0,NULL);}static void mg(void*p){note(3,id(p),0,NULL);}static void bots(void){note(4,0,0,NULL);}
static void debug(const char*f,int n,int state){note(5,n,state,f);}
static void get(int n,char*b,int len){note(6,n,len,NULL);Q_strncpyz(b,"\\x\\17\\t\\257",len);}
static void set(int n,const char*s){note(7,n,0,s);}
static int box(const float*a,const float*b,int*list,int max){int i;note(8,max,0,NULL);for(i=0;i<3;i++)if(a[i]!=-10-i||b[i]!=10+i)bad=1;for(i=0;i<7;i++)list[i]=11+i;return variant&128?0:7;}
static void*position(void*p){int n=id(p);note(9,n,0,NULL);if((variant&16)&&n==11)return NULL;if((variant&32)&&n==12)return entity(17);return entity(10);}
static void drop(void*p){note(10,id(p),0,NULL);}static void freeEnt(void*p){note(11,id(p),0,NULL);}
static void*temp(const float*o,int ev){note(12,ev,0,NULL);if(o[0]<11||o[0]>17||o[1]!=2||o[2]!=3)bad=1;return entity(17);}
static float damage(void*t,void*i,void*a,float*d,float*p,int amount,int flags,int mod){note(13,id(t),amount,NULL);note(14,flags,mod,NULL);if(i!=entity(10)||a!=entity(10)||d||p)bad=1;return 1;}
static int listid(void*p){if(p==(side?(void*)&mapEntityData[0]:(void*)(base+0x76c280)))return 0;if(p==(side?(void*)&mapEntityData[1]:(void*)(base+0x7782b4)))return 1;bad=1;return 0;}
static void*findmap(void*p,int n){int t=listid(p);note(15,t,n,NULL);return variant&(1<<t)?NULL:(side?(void*)&marker[t]:(void*)rawMarker[t]);}
static void*freemap(void*p,void*m){int t=listid(p);note(16,t,0,NULL);if(m!=(side?(void*)&marker[t]:(void*)rawMarker[t]))bad=1;return NULL;}
typedef struct{int state,power,flags,clip,realclip,contents,realcontents,nonsolid,count2,n;event log[128];}result;
static void init(int old,int type,int v){int n,k;gentity_t*e;byte*r;int types[]={ET_PLAYER,ET_ITEM,ET_MISSILE,ET_CORPSE,ET_GENERAL,ET_ITEM,ET_MISSILE};
 for(n=10;n<18;n++){e=&g_entities[n];r=base+0x226e2a0+n*0x57c;memset(e,0,sizeof(*e));memset(r,0,0x57c);e->s.number=*(int*)r=n;e->classname="ordinary";*(char**)(r+0x1a4)="ordinary";if(n>10){e->s.eType=*(int*)(r+4)=types[n-11];e->physicsObject=*(int*)(r+0x1cc)=((v&4)!=0);e->s.origin[0]=*(float*)(r+0x5c)=(float)n;e->s.origin[1]=*(float*)(r+0x60)=2;e->s.origin[2]=*(float*)(r+0x64)=3;e->item=&item;*(void**)(r+0x33c)=rawItem;e->methodOfDeath=*(int*)(r+0x2dc)=n==13?45:0;if(n==11||(v&8)){e->client=(gclient_t*)1;*(void**)(r+0x190)=(void*)1;}}}
 item.giType=*(int*)(rawItem+0x24)=v&64?IT_TEAM:IT_HEALTH;
 e=&g_entities[10];r=base+0x226e2a0+10*0x57c;e->entstate=*(int*)(r+0x47c)=old;e->s.powerups=*(int*)(r+0xe4)=19;e->s.eType=*(int*)(r+4)=type;e->s.eFlags=*(int*)(r+8)=v&2?2:0;e->clipmask=*(int*)(r+0x1d4)=123;e->realClipmask=*(int*)(r+0x1d8)=456;e->r.contents=*(int*)(r+0x14c)=789;e->realContents=*(int*)(r+0x1dc)=987;e->realNonSolidBModel=*(int*)(r+0x1e0)=v&1;e->count=*(int*)(r+0x2e4)=42;e->count2=*(int*)(r+0x3a4)=v;
 e->classname=v&4?"MiSc_Mg42":"ordinary";*(char**)(r+0x1a4)=e->classname;for(k=0;k<3;k++){e->r.absmin[k]=*(float*)(r+0x150+k*4)=(float)(-10-k);e->r.absmax[k]=*(float*)(r+0x15c+k*4)=(float)(10+k);}}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*original)(void*,int);int old,state,ix,v,n=0;int types[]={ET_GENERAL,ET_WOLF_OBJECTIVE,ET_COMMANDMAP_MARKER,ET_CONSTRUCTIBLE};result a,b;gentity_t*e;byte*r;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;original=(void*)(base+0x95440);
#define HOOK(rva,fn,repl) if(!hook(base+rva,repl)||!hook(fn,repl))return 2
 HOOK(0x8afa0,trap_LinkEntity,link);HOOK(0x8afc0,trap_UnlinkEntity,unlinker);HOOK(0x68e50,mg42_stopusing,mg);HOOK(0x1c880,BotCalculateMg42Spots,bots);HOOK(0x5edc0,G_DPrintf,debug);HOOK(0x8ade0,trap_GetConfigstring,get);HOOK(0x8adc0,trap_SetConfigstring,set);HOOK(0x8afe0,trap_EntitiesInBox,box);HOOK(0x6dc00,G_TestEntityPosition,position);HOOK(0x8db40,Team_DroppedFlagThink,drop);HOOK(0x94f30,G_FreeEntity,freeEnt);HOOK(0x94fa0,G_TempEntity,temp);HOOK(0x589c0,G_Damage,damage);HOOK(0x90250,G_FindMapEntityData,findmap);HOOK(0x90190,G_FreeMapEntityData,freemap);
 for(old=-1;old<=6;old++)for(state=-1;state<=6;state++)for(ix=0;ix<4;ix++)for(v=0;v<256;v++)for(side=0;side<2;side++){
 variant=v;bad=count=0;memset(events,0,sizeof(events));init(old,types[ix],v);e=&g_entities[10];r=base+0x226e2a0+10*0x57c;if(side)G_SetEntState(e,state);else original(r,state);memset(&b,0,sizeof(b));
#define FIELD(name,member,off) b.name=side?e->member:*(int*)(r+off)
 FIELD(state,entstate,0x47c);FIELD(power,s.powerups,0xe4);FIELD(flags,s.eFlags,8);FIELD(clip,clipmask,0x1d4);FIELD(realclip,realClipmask,0x1d8);FIELD(contents,r.contents,0x14c);FIELD(realcontents,realContents,0x1dc);FIELD(nonsolid,realNonSolidBModel,0x1e0);FIELD(count2,count2,0x3a4);b.n=count;memcpy(b.log,events,sizeof(events));
 if(!side)a=b;else{if(bad||memcmp(&a,&b,sizeof(a))){int q;printf("state mismatch old%d new%d type%d variant%d events%d/%d bad%d\n",old,state,types[ix],v,a.n,b.n,bad);for(q=0;q<a.n||q<b.n;q++)printf("%d: %d %d %d [%s] / %d %d %d [%s]\n",q,a.log[q].k,a.log[q].a,a.log[q].b,a.log[q].text,b.log[q].k,b.log[q].a,b.log[q].b,b.log[q].text);return 1;}n++;}}
 printf("PASS G_SetEntState %d whole controller cases\n",n);return 0;
}
