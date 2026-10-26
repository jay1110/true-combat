#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern void SP_corona(gentity_t*),use_corona(gentity_t*,gentity_t*,gentity_t*);
static byte raw[0x57c],*base;static gentity_t ent;static int side,count,bad,variant;
typedef struct {int k,a;char key[32],def[32];}rec;static rec logs[24];
static const char*keys[]={"scale","coneShader","insects","type","blend","cone_radius","cone_type","cone_height","altitude","degrees"};
static const float numbers[]={-33,-1,0,.1f,.99999994f,1,1.00000012f,31.9f,32,255,256,1000};
static int hook(void*a,void*f){DWORD x,y;byte*p=a;if(!VirtualProtect(a,5,PAGE_EXECUTE_READWRITE,&x))return 0;p[0]=0xe9;*(int*)(p+1)=(byte*)f-p-5;FlushInstructionCache(GetCurrentProcess(),a,5);VirtualProtect(a,5,x,&y);return 1;}
static int note(int k,const char*key,const char*def){int i;rec*r;if(count>=24)exit(3);r=&logs[count++];memset(r,0,sizeof(*r));r->k=k;Q_strncpyz(r->key,key,32);if(def)Q_strncpyz(r->def,def,32);for(i=0;i<10;i++)if(!strcmp(key,keys[i]))return i;return -1;}
static int integer(const char*k,const char*d,int*out,const char*f,int l){int i=note(1,k,d);if(i<0){bad=1;return 0;}*out=(variant&(1<<i))?(int)numbers[((variant>>(i%5))+i)%12]:atoi(d);return (variant&(1<<i))!=0;}
static int floating(const char*k,const char*d,float*out,const char*f,int l){int i=note(2,k,d);if(i<0){bad=1;return 0;}*out=(variant&(1<<i))?numbers[((variant>>(i%5))+i)%12]:(float)atof(d);return (variant&(1<<i))!=0;}
static int string(const char*k,const char*d,char**out,const char*f,int l){int i=note(3,k,d);if(i!=1)bad=1;*out=(variant&2)?((variant&4)?"":"textures/testcone"):(char*)d;return (variant&2)!=0;}
static int shader(char*s){note(4,s,NULL);return 217;}
static void link(void*p){note(5,"link",NULL);if(p!=(side?(void*)&ent:(void*)raw))bad=1;logs[count-1].a=side?ent.s.clientNum:*(int*)(raw+0xac);}
int main(int argc,char**argv){HMODULE dll;int flags,col,v,j,n=0;rec expected[24];int ec;float colors[4][3]={{0,0,0},{-1,-2,-3},{.25f,.5f,1},{-1,1.001f,.33333334f}};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;
 if(!hook(base+0x85a30,integer)||!hook(G_SpawnIntExt,integer)||!hook(base+0x859f0,floating)||!hook(G_SpawnFloatExt,floating)||!hook(base+0x85940,string)||!hook(G_SpawnStringExt,string)||!hook(base+0x94830,shader)||!hook(G_ShaderIndex,shader)||!hook(base+0x8afa0,link)||!hook(trap_LinkEntity,link))return 2;
 for(flags=0;flags<32;flags++)for(v=0;v<1024;v++)for(col=0;col<4;col++){
  memset(raw,0x25,sizeof(raw));memset(&ent,0x25,sizeof(ent));variant=v;ent.spawnflags=*(int*)(raw+0x1a8)=flags;memcpy(ent.dl_color,colors[col],12);memcpy(raw+0x350,colors[col],12);
  side=0;bad=count=0;((void(__cdecl*)(void*))(base+0x67690))(raw);ec=count;memcpy(expected,logs,sizeof(expected));side=1;count=0;SP_corona(&ent);
  if(bad||ec!=count||memcmp(expected,logs,count*sizeof(rec))){fprintf(stderr,"corona flags%d variant%d color%d calls%d/%d failed\n",flags,v,col,count,ec);return 1;}
#define CHECK(field,offset) if(ent.field!=*(int*)(raw+offset)){fprintf(stderr,"corona flags%d variant%d color%d "#field" %d/%d failed\n",flags,v,col,ent.field,*(int*)(raw+offset));return 1;}
  CHECK(s.eType,4);CHECK(s.dl_intensity,0x9c);CHECK(s.density,0xf4);CHECK(s.modelindex,0xa4);CHECK(s.modelindex2,0xa8);CHECK(s.otherEntityNum2,0x90);CHECK(s.effect3Time,0x114);CHECK(s.effect2Time,0x110);CHECK(s.effect1Time,0x10c);CHECK(s.frame,0xb0);CHECK(s.dmgFlags,0xf8);CHECK(s.clientNum,0xac);CHECK(s.eventParm,0xbc);CHECK(r.svFlags,0x128);
  if(memcmp(ent.dl_color,raw+0x350,12)||ent.use!=use_corona||*(void**)(raw+0x2b4)!=base+0x67660)return 1;n++;
 }
 printf("%d complete SP_corona original spawn comparisons passed\n",n);return 0;
}
