#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
extern qboolean G_ScriptAction_SetState(gentity_t*,char*);
extern void SP_func_obj_touch(gentity_t*);
extern void InitTrigger(gentity_t*);
static byte r[3][0x57c],rp[0x57c],ro[0x57c],rc[0x2000],*base;static gentity_t e[3],parent,other;static gclient_t client;
static int side,num,finds,count,bad;static jmp_buf jump;
typedef struct{int kind,a,b;char text[256];}record;static record records[40];
static int indexOf(void*p){int i;for(i=0;i<3;i++)if(p==(side?(void*)&e[i]:(void*)r[i]))return i;bad=1;return -1;}
static void note(int k,int a,int b,const char*s){record*t;if(count>=40)exit(3);t=&records[count++];memset(t,0,sizeof(*t));t->kind=k;t->a=a;t->b=b;if(s)Q_strncpyz(t->text,s,sizeof(t->text));}
static int hook(void*p,void*f){DWORD o,t;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);return 1;}
static void*find(void*from,const char*name,int hash){void*want=finds?(side?(void*)&e[finds-1]:(void*)r[finds-1]):(side?(void*)&g_entities[63]:(void*)(base+0x226e2a0+63*0x57c));if(from!=want||hash!=BG_StringHashValue(name))bad=1;note(1,finds,hash,name);if(finds==num)return NULL;return side?(void*)&e[finds++]:(void*)r[finds++];}
static void fatal(const char*s){note(2,0,0,s);longjmp(jump,1);}static void print(const char*s,...){note(3,0,0,s);}static void debug(const char*s,...){note(4,0,0,s);}
static void link(void*p){note(5,indexOf(p),0,NULL);}static void unlinkEntityHook(void*p){note(6,indexOf(p),0,NULL);}static int box(const float*a,const float*b,int*list,int max){note(7,max,0,NULL);return 0;}static void bots(void){note(8,0,0,NULL);}static void init(void*p){}
static void script(void*p,const char*s,const char*v){note(9,indexOf(p),0,s);note(10,0,0,v);}static void use(void*p,void*q){if(q!=(side?(void*)&other:(void*)ro))bad=1;note(11,indexOf(p),0,NULL);}
int main(int argc,char**argv){HMODULE dll;int(__cdecl*original)(void*,char*);void(__cdecl*touch)(void*,void*,void*);int old,ix,variant,n=0,j;
 const char*inputs[]={"flag default","flag invisible","flag underconstruction","flag dynamited","flag inactive","flag locked","flag LOCKED","flag locked trailing","flag bogus","flag","","\"flag\" \"locked\""};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;original=(void*)(base+0x83090);touch=(void*)(base+0x75860);
 if(!hook(base+0x949f0,find)||!hook(G_FindByTargetnameFast,find)||!hook(base+0x8ab30,fatal)||!hook(trap_Error,fatal)||!hook(base+0x5ed80,print)||!hook(G_Printf,print)||!hook(base+0x5edc0,debug)||!hook(G_DPrintf,debug)||!hook(base+0x8afa0,link)||!hook(trap_LinkEntity,link)||!hook(base+0x8afc0,unlinkEntityHook)||!hook(trap_UnlinkEntity,unlinkEntityHook)||!hook(base+0x8afe0,box)||!hook(trap_EntitiesInBox,box)||!hook(base+0x1c880,bots)||!hook(BotCalculateMg42Spots,bots)||!hook(InitTrigger,init)||!hook(base+0x7d4b0,script)||!hook(G_Script_ScriptEvent,script)||!hook(base+0x94b90,use)||!hook(G_UseTargets,use))return 2;
 for(old=0;old<6;old++)for(ix=0;ix<12;ix++)for(variant=0;variant<64;variant++){
  record expected[40];int ec,x=-1,y=-1;char a[128],b[128];num=variant&3;memset(r,0,sizeof(r));memset(e,0,sizeof(e));memset(rp,0,sizeof(rp));memset(&parent,0,sizeof(parent));memset(ro,0,sizeof(ro));memset(rc,0,sizeof(rc));memset(&other,0,sizeof(other));memset(&client,0,sizeof(client));
  other.client=&client;*(void**)(ro+0x190)=rc;client.sess.sessionTeam=*(int*)(rc+0xb30)=(variant>>2)&3;parent.s.eType=*(int*)(rp+4)=ET_OID_TRIGGER;level.time=*(int*)(base+0x7febe0)=100000;
  for(j=0;j<3;j++){
   SP_func_obj_touch(&e[j]);e[j].classname="func_obj_touch";*(char**)(r[j]+0x1a4)=e[j].classname;e[j].s.eType=*(int*)(r[j]+4)=63;
   e[j].entstate=*(int*)(r[j]+0x47c)=old;e[j].s.powerups=*(int*)(r[j]+0xe4)=old;e[j].parent=&parent;*(void**)(r[j]+0x218)=rp;
   e[j].clipmask=*(int*)(r[j]+0x1d4)=77;e[j].r.contents=*(int*)(r[j]+0x14c)=88;e[j].realClipmask=*(int*)(r[j]+0x1d8)=99;e[j].realContents=*(int*)(r[j]+0x1dc)=111;
   e[j].realNonSolidBModel=*(int*)(r[j]+0x1e0)=(variant>>4)&1;e[j].s.eFlags=*(int*)(r[j]+8)=(variant&16)?EF_NONSOLID_BMODEL:0;
   e[j].eventTime=*(int*)(r[j]+0x1c0)=100000+((variant&32)?1:0);e[j].freetime=*(int*)(r[j]+0x1bc)=200000;
  }
  Q_strncpyz(a,inputs[ix],sizeof(a));Q_strncpyz(b,inputs[ix],sizeof(b));side=0;bad=count=finds=0;if(!setjmp(jump)){x=original(NULL,a);if(num)touch(r[0],ro,NULL);}if(bad)return 3;ec=count;memcpy(expected,records,sizeof(expected));
  side=1;bad=count=finds=0;if(!setjmp(jump)){y=G_ScriptAction_SetState(NULL,b);if(num)e[0].touch(&e[0],&other,NULL);}
  if(bad||x!=y||ec!=count||memcmp(expected,records,count*sizeof(record))){fprintf(stderr,"capture old%d input%d variant%d events%d/%d result%d/%d failed\n",old,ix,variant,count,ec,y,x);return 1;}
  for(j=0;j<3;j++)if(e[j].entstate!=*(int*)(r[j]+0x47c)||e[j].s.powerups!=*(int*)(r[j]+0xe4)||e[j].eventTime!=*(int*)(r[j]+0x1c0)||e[j].freetime!=*(int*)(r[j]+0x1bc)||e[j].clipmask!=*(int*)(r[j]+0x1d4)||e[j].r.contents!=*(int*)(r[j]+0x14c)||e[j].s.eFlags!=*(int*)(r[j]+8)||e[j].realClipmask!=*(int*)(r[j]+0x1d8)||e[j].realContents!=*(int*)(r[j]+0x1dc)||e[j].realNonSolidBModel!=*(int*)(r[j]+0x1e0)){fprintf(stderr,"capture state old%d input%d variant%d ent%d failed\n",old,ix,variant,j);return 1;}n++;
 }
 printf("%d original Capture parser/state/touch chain comparisons passed\n",n);return 0;
}

