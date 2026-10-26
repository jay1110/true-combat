#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
extern qboolean G_ScriptAction_SetPosition(gentity_t*,char*);
extern qboolean G_ScriptAction_ChangeModel(gentity_t*,char*);
static byte raw[0x57c],rc[0x2000],rawTarget[0x57c];static gentity_t ent,target;static gclient_t client;static pathCorner_t corner;
static int side,scene,count;static jmp_buf jump;
typedef struct {int kind,value;char text[1024];} event_t;static event_t events[8];
static const char*inputs[]={NULL,""," ","\n","spot","\"spot two\" trailing","// c\nspot","/* c */ spot","models/gun.md3","models/a.b.c","noextension","\"\"",".hidden","path/../gun.mdc","MODEL.MD3","name trailing"};
static int hook(void*p,void*f){DWORD o,t;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);return 1;}
static void record(int k,int v,const char*s){event_t*e;if(count>=8)exit(3);e=&events[count++];memset(e,0,sizeof(*e));e->kind=k;e->value=v;Q_strncpyz(e->text,s?s:"",sizeof(e->text));}
static void fatal(const char*s){record(5,0,s);longjmp(jump,1);}
static void*findPath(const char*s){record(1,0,s);return scene&1?&corner:NULL;}
static void*findEntity(void*p,const char*s){if(p)exit(3);record(2,0,s);return scene&2?(side?(void*)&target:(void*)rawTarget):NULL;}
static int loadTag(const char*s){record(3,scene-64,s);return scene-64;}
static int model(const char*s){record(4,scene+11,s);return scene+11;}
int main(int argc,char**argv){HMODULE dll;byte*b;int(__cdecl*fn[2])(void*,char*);int mode,sc,ix,n=0,j;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;fn[0]=(void*)(b+0x7e1d0);fn[1]=(void*)(b+0x7e390);
 if(!hook(b+0x2dd30,findPath)||!hook(BG_Find_PathCorner,findPath)||!hook(b+0x94960,findEntity)||!hook(G_FindByTargetname,findEntity)||!hook(b+0x8b0e0,loadTag)||!hook(trap_LoadTag,loadTag)||!hook(b+0x947d0,model)||!hook(G_ModelIndex,model)||!hook(b+0x8ab30,fatal)||!hook(trap_Error,fatal))return 2;
 for(mode=0;mode<2;mode++)for(sc=0;sc<128;sc++)for(ix=0;ix<16;ix++){
  char a[256],c[256];event_t expected[8];int expectedCount,x=-1,y=-1;scene=sc;
  memset(raw,0,sizeof(raw));memset(rc,0,sizeof(rc));memset(&ent,0,sizeof(ent));memset(&client,0,sizeof(client));
  ent.client=sc&4?&client:NULL;*(void**)(raw+0x190)=sc&4?rc:NULL;
  for(j=0;j<3;j++){corner.origin[j]=(float)(sc-j)/3;target.r.currentOrigin[j]=((float*)(rawTarget+0x168))[j]=(float)(j+sc)/7;client.ps.origin[j]=((float*)(rc+0x14))[j]=123+j;}
  ent.tagNumber=*(int*)(raw+0x4f4)=77;ent.s.modelindex2=*(int*)(raw+0xa8)=99;
  if(inputs[ix]){Q_strncpyz(a,inputs[ix],sizeof(a));Q_strncpyz(c,inputs[ix],sizeof(c));}
  side=0;count=0;if(!setjmp(jump))x=fn[mode](raw,inputs[ix]?a:NULL);expectedCount=count;memcpy(expected,events,sizeof(expected));
  side=1;count=0;if(!setjmp(jump))y=mode?G_ScriptAction_ChangeModel(&ent,inputs[ix]?c:NULL):G_ScriptAction_SetPosition(&ent,inputs[ix]?c:NULL);
  if(x!=y||count!=expectedCount||memcmp(expected,events,count*sizeof(event_t))||memcmp(ent.r.currentOrigin,raw+0x168,12)||memcmp(ent.s.origin,raw+0x5c,12)||memcmp(ent.s.pos.trBase,raw+0x18,12)||memcmp(client.ps.origin,rc+0x14,12)||ent.s.pos.trType!=*(int*)(raw+0xc)||ent.s.pos.trTime!=*(int*)(raw+0x10)||memcmp(ent.s.pos.trDelta,raw+0x24,12)||ent.tagNumber!=*(int*)(raw+0x4f4)||ent.s.modelindex2!=*(int*)(raw+0xa8)){
   fprintf(stderr,"script transform mode%d scene%d input%d result%d/%d events%d/%d failed\n",mode,sc,ix,y,x,count,expectedCount);return 1;
  }n++;
 }
 printf("%d whole position/model script comparisons passed (2048 each, fatal included)\n",n);return 0;
}
