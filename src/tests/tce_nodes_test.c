#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include "../game/tce_nodes.h"
#include <windows.h>
#include <stdio.h>
static byte*base,stream[4096],snapshot[sizeof(tceNodes)];static int side,variant,pos,size,count,bad;
typedef struct{int type,a;vec3_t start,end;char s[128];}event;static event events[256],expected[256];
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static void note(int t,int a,const char*s){event*e;if(count>=256)exit(3);e=&events[count++];memset(e,0,sizeof(*e));e->type=t;e->a=a;if(s)Q_strncpyz(e->s,s,sizeof(e->s));}
static void trace(trace_t*t,const float*s,const float*mi,const float*ma,const float*e,int pass,int mask){note(1,pass,NULL);if(mi||ma||mask!=0x10001)bad=1;memcpy(events[count-1].start,s,12);memcpy(events[count-1].end,e,12);memset(t,0,sizeof(*t));t->fraction=(variant+count)%3==0?1.0f:(variant+count)%3==1?.5f:0;}
static int openFile(const char*s,int*h,int mode){note(2,mode,s);pos=0;*h=variant%7==0?0:13;return *h?size:-1;}
static void readFile(void*d,int n,int h){note(3,n,NULL);if(h!=13||pos+n>size){bad=1;memset(d,0,n);return;}memcpy(d,stream+pos,n);pos+=n;}
static void closeFile(int h){note(4,h,NULL);}
static void cvar(vmCvar_t*c,const char*n,const char*d,int f){note(5,f,n);if(strcmp(d,""))bad=1;memset(c,0,sizeof(*c));strcpy(c->string,"obj_northport");}
static void QDECL print(const char*f,...){va_list a;char b[128];va_start(a,f);Q_vsnprintf(b,sizeof(b),f,a);va_end(a);note(6,0,b);}
static void append(const void*p,int n){memcpy(stream+size,p,n);size+=n;}
static void makeStream(void){short n=variant%5,i,j;float version=variant%3==0?1:2;char magic[11]="truecombat";size=0;if(variant%11==0)magic[0]='x';append(magic,11);append(&version,4);append(&n,2);for(i=0;i<n;i++){vec3_t o;short flags=(short)(i+1),entities[3]={1,2,3},links=(short)(i%3);int packed=0x00070080+i;VectorSet(o,i*32,variant,-i*16);append(&i,2);append(o,12);append(&flags,2);append(entities,6);append(&packed,4);append(&links,2);for(j=0;j<links;j++){short to=(short)((i+j)%n),f=(short)(16+j);append(&to,2);append(&f,2);}}}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*clear)(void),(__cdecl*update)(void),(__cdecl*init)(void);int(__cdecl*add)(const float*,short,const short*,int),(__cdecl*connect)(int,int,short),(__cdecl*loadNodes)(const char*);int phase,v,i,j,ret,want=0,ec=0,n=0;short wantCount=0;int wantLast=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;if(sizeof(tceNode_t)!=0x6c)return 3;clear=(void*)(base+0x40190);add=(void*)(base+0x40040);connect=(void*)(base+0x40120);loadNodes=(void*)(base+0x40700);init=(void*)(base+0x40210);update=(void*)(base+0x412d0);
#define H(r,fn,repl) if(!hook(base+r,repl)||!hook(fn,repl))return 2
 H(0x8ae80,trap_Trace,trace);H(0x8ab90,trap_FS_FOpenFile,openFile);H(0x8abb0,trap_FS_Read,readFile);H(0x8abf0,trap_FS_FCloseFile,closeFile);H(0x8ac50,trap_Cvar_Register,cvar);H(0x5ed80,G_Printf,print);
 for(phase=0;phase<6;phase++)for(v=0;v<512;v++)for(side=0;side<2;side++){
  tceNode_t*nodes=side?tceNodes:(void*)(base+0x7e2500);variant=v;count=bad=pos=0;ret=0;memset(events,0,sizeof(events));memset(tceNodes,0xa5,sizeof(tceNodes));memset(base+0x7e2500,0xa5,sizeof(tceNodes));tceNumNodes=*(short*)(base+0x247d74)=0;tceLastNode=*(int*)(base+0xc0a50)=123;g_developer.integer=*(int*)(base+0x6d99ac)=v&1;makeStream();
  if(phase==0){if(side)TCE_ClearNodes();else clear();}
  else{if(side)TCE_ClearNodes();else clear();
   if(phase==1){vec3_t o;short e[3]={0,17,1023};int indices[]={0,1,1023,1024};tceNumNodes=*(short*)(base+0x247d74)=(short)indices[v%4];VectorSet(o,v,-v,12.5f);ret=side?TCE_AddNode(o,(short)(v%8-1),e,(v<<8)|v):add(o,(short)(v%8-1),e,(v<<8)|v);}
   if(phase==2){nodes[3].flags=(short)(v%5-1);nodes[3].numLinks=(short)(v%13);ret=side?TCE_ConnectNodes(3,v%7==0?-1:v%1024,(short)v):connect(3,v%7==0?-1:v%1024,(short)v);}
   if(phase==3){tceNumNodes=*(short*)(base+0x247d74)=4;for(i=0;i<4;i++){VectorSet(nodes[i].origin,i*32,i*-16,24);nodes[i].flags=(short)(v&63);nodes[i].entities[0]=(short)(v%3==0?0:1);nodes[i].entities[1]=2;nodes[i].entities[2]=1023;nodes[i].moverType=(short)(v%2);nodes[i].moverEntity=3;nodes[i].numLinks=(short)(v%13);for(j=0;j<nodes[i].numLinks;j++){nodes[i].links[j].target=(short)((i+j)%4);nodes[i].links[j].flags=(short)((j&1)?0x1010:0x20);} }for(i=0;i<4;i++)g_entities[i].spawnflags=*(int*)(base+0x226e2a0+i*0x57c+0x1a8)=(v>>i)&1;if(side)TCE_UpdateNodes();else update();}
   if(phase==4)ret=side?TCE_LoadNodes("botroutes/obj_northport.wps"):loadNodes("botroutes/obj_northport.wps");
   if(phase==5){if(side)TCE_InitNodes();else init();}
  }
  if(!side){memcpy(snapshot,nodes,sizeof(snapshot));memcpy(expected,events,sizeof(events));want=ret;ec=count;wantCount=*(short*)(base+0x247d74);wantLast=*(int*)(base+0xc0a50);}else{if(bad||ret!=want||count!=ec||memcmp(snapshot,nodes,sizeof(snapshot))||memcmp(expected,events,sizeof(events))||wantCount!=tceNumNodes||wantLast!=tceLastNode){printf("Nodes mismatch phase%d case%d bad%d result%d/%d calls%d/%d count%d/%d\n",phase,v,bad,want,ret,ec,count,wantCount,tceNumNodes);for(i=0;i<sizeof(snapshot);i++)if(snapshot[i]!=((byte*)nodes)[i]){printf("node byte%x %02x/%02x\n",i,snapshot[i],((byte*)nodes)[i]);break;}for(i=0;i<ec||i<count;i++)if(memcmp(&expected[i],&events[i],sizeof(event)))printf("event%d %d,%d[%s]/%d,%d[%s]\n",i,expected[i].type,expected[i].a,expected[i].s,events[i].type,events[i].a,events[i].s);return 1;}n++;}
 }
 printf("PASS six node controllers %d cases (512 per function)\n",n);return 0;
}
