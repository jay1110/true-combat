#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include "../game/tce_nodes.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
extern short TCE_FindClosestNodeToPoint(gentity_t*,const vec3_t);
extern void TCE_ShowNode(int,int);
static const short flagCases[]={0,16,32,48,-1};
static int originalRun,traceCount,tempCount,printCount,traceMode;static byte rawTemps[400][0x400];static gentity_t nativeTemps[400];
typedef struct{vec3_t from,to;int pass,mask,nullBounds;} traceCall_t;static traceCall_t traces[1024];
typedef struct{vec3_t origin,origin2;int event,box,highlight;} output_t;static int eventCodes[400];static char printText[256];
static void trace(trace_t*t,const vec3_t from,const vec3_t mins,const vec3_t maxs,const vec3_t to,int pass,int mask){traceCall_t*c=&traces[traceCount];memset(c,0,sizeof(*c));VectorCopy(from,c->from);VectorCopy(to,c->to);c->pass=pass;c->mask=mask;c->nullBounds=(!mins)+2*(!maxs);memset(t,0,sizeof(*t));t->fraction=traceMode==0?1.0f:traceMode==1?.5f:(traceCount%3?1.0f:.999f);traceCount++;}
static gentity_t*temp(vec3_t org,int ev){int i=tempCount++;eventCodes[i]=ev;if(originalRun){memset(rawTemps[i],0,sizeof(rawTemps[i]));memcpy(rawTemps[i]+0x5c,org,12);return(void*)rawTemps[i];}memset(&nativeTemps[i],0,sizeof(nativeTemps[i]));VectorCopy(org,nativeTemps[i].s.origin);return&nativeTemps[i];}
static void print(const char*fmt,...){va_list ap;printCount++;va_start(ap,fmt);Q_vsnprintf(printText,sizeof(printText),fmt,ap);va_end(ap);}
static void capture(output_t*out){int i;for(i=0;i<tempCount;i++){memset(&out[i],0,sizeof(out[i]));out[i].event=eventCodes[i];if(originalRun){memcpy(out[i].origin,rawTemps[i]+0x5c,12);memcpy(out[i].origin2,rawTemps[i]+0x68,12);out[i].box=*(int*)(rawTemps[i]+0xf8);out[i].highlight=*(int*)(rawTemps[i]+0x114);}else{VectorCopy(nativeTemps[i].s.origin,out[i].origin);VectorCopy(nativeTemps[i].s.origin2,out[i].origin2);out[i].box=nativeTemps[i].s.dmgFlags;out[i].highlight=nativeTemps[i].s.effect3Time;}}}
static void hook(void*at,void*fn){DWORD old,t;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t);}
int main(int argc,char**argv){HMODULE m;byte*b;short(__cdecl*nearest)(void*,const float*);void(__cdecl*show)(int,int);static byte entity[0x400],client[0x3000];gentity_t ent;gclient_t cl;vec3_t point;int k,i,j,n,np;short expected,actual;static traceCall_t prior[1024];static output_t expectedOut[400],actualOut[400];char priorPrint[256];if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;b=(byte*)m;nearest=(void*)(b+0x41450);show=(void*)(b+0x41760);hook(b+0x8ae80,trace);hook(trap_Trace,trace);hook(b+0x94fa0,temp);hook(G_TempEntity,temp);hook(b+0x5ed80,print);hook(G_Printf,print);
 memset(&ent,0,sizeof(ent));memset(&cl,0,sizeof(cl));ent.client=&cl;*(void**)(entity+0x190)=client;
 for(k=0;k<4096;k++){
  tceNumNodes=k%17;*(short*)(b+0x247d74)=tceNumNodes;memset(tceNodes,0,sizeof(tceNodes));
  for(i=0;i<tceNumNodes;i++){for(j=0;j<3;j++)tceNodes[i].origin[j]=(float)((i+1)*(j+1))*0.37f;tceNodes[i].flags=flagCases[((k/17)+i)%5];if(k%7==0)VectorCopy(tceNodes[0].origin,tceNodes[i].origin);}
  memcpy(b+0x7e2500,tceNodes,sizeof(tceNodes));ent.s.number=*(int*)entity=k%64;cl.sess.sessionTeam=*(int*)(client+0xb30)=(k/85)%4;VectorSet(point,.13f*(k%13),-.29f,1.11f);traceMode=(k/340)%3;
  originalRun=1;traceCount=0;expected=nearest(entity,point);n=traceCount;memcpy(prior,traces,n*sizeof(prior[0]));originalRun=0;traceCount=0;actual=TCE_FindClosestNodeToPoint(&ent,point);if(expected!=actual||n!=traceCount||memcmp(prior,traces,n*sizeof(prior[0]))){printf("nearest case%d result%d/%d\n",k,expected,actual);return 1;}
 }
 for(k=0;k<2048;k++){
  tceNumNodes=8;*(short*)(b+0x247d74)=8;memset(tceNodes,0,sizeof(tceNodes));for(i=0;i<8;i++){for(j=0;j<3;j++)tceNodes[i].origin[j]=(float)(k%23+i*17-j*9)*.37f;tceNodes[i].flags=(k>>i)&1;tceNodes[i].numLinks=(k+i)%4;for(j=0;j<tceNodes[i].numLinks;j++)tceNodes[i].links[j].target=(i+j+1)%8;}memcpy(b+0x7e2500,tceNodes,sizeof(tceNodes));
  originalRun=1;tempCount=printCount=0;printText[0]=0;show(k%8,(k/8)%3-1);capture(expectedOut);n=tempCount;np=printCount;strcpy(priorPrint,printText);
  originalRun=0;tempCount=printCount=0;printText[0]=0;TCE_ShowNode(k%8,(k/8)%3-1);capture(actualOut);if(n!=tempCount||np!=printCount||strcmp(priorPrint,printText)||memcmp(expectedOut,actualOut,n*sizeof(output_t))){printf("shownode case%d events%d/%d\n",k,n,tempCount);return 1;}
 }puts("6144 node editor comparisons passed:4096 nearest,2048 display");return 0;}

