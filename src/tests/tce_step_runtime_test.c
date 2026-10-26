/* Execute the complete production step controller against TC's original DLL.
 * Both receive the same scripted collision/slide responses; compare all trace
 * arguments, resulting player state, event choice and dependency call counts. */
#include "../game/bg_slidemove.c"
#include <windows.h>
#include <stdio.h>
static playerState_t *active;
static int scenario, traces, slides, events, eventValue, gravityValue;
static float vectors[6][12];
static int args[6][2];
static void patch(void *at, void *fn) {
 DWORD old,tmp;unsigned char *p=at;
 VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;
 *(int *)(p+1)=(unsigned char *)fn-p-5;
 FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);
}
static qboolean slide(qboolean gravity) {
 ++slides;gravityValue=gravity;
 active->origin[0]+=3;active->origin[1]-=2;
 active->velocity[2]=(float)((scenario%3)-1);
 return (scenario&1)?qtrue:qfalse;
}
static void event(int value) {++events;eventValue=value;}
static void collision(trace_t *tr,const vec3_t a,const vec3_t b,const vec3_t c,const vec3_t d,int cl,int mask) {
 int i,n=traces++;if(n>=6)exit(9);
 memcpy(vectors[n],a,12);memcpy(vectors[n]+3,b,12);memcpy(vectors[n]+6,c,12);memcpy(vectors[n]+9,d,12);
 args[n][0]=cl;args[n][1]=mask;memset(tr,0,sizeof(*tr));
 tr->allsolid=(scenario>>(n+1))&1;tr->startsolid=(scenario>>(n+7))&1;
 tr->fraction=(float)((scenario>>12)%3)*.5f;
 tr->plane.normal[2]=(scenario&128)?.7f:1.0f;
 for(i=0;i<3;++i)tr->endpos[i]=a[i]+tr->fraction*(d[i]-a[i]);
}
static void reset(void){traces=slides=events=eventValue=gravityValue=0;memset(vectors,0,sizeof(vectors));memset(args,0,sizeof(args));}
int main(int argc,char **argv) {
 HMODULE dll;unsigned char *base,raw[0x700],rp[0x200];
 playerState_t ps;pmove_t move;int n,et,es,ee,ev,eg,ea[6][2],server;float expected[6][12];
 void (__cdecl *original)(qboolean);
 if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;
 base=(unsigned char *)dll;server=strstr(argv[1],"qagame")!=NULL;original=(void *)(base+(server?0x3c040:0x13d00));
 patch(base+(server?0x3b930:0x135f0),slide);patch(PM_SlideMove,slide);
 patch(base+(server?0x309f0:0x8680),event);patch(PM_AddEvent,event);gearDef.parsed=1;
 for(n=0;n<24576;++n) {
  scenario=n;memset(&ps,0,sizeof(ps));memset(&move,0,sizeof(move));memset(raw,0,sizeof(raw));memset(rp,0,sizeof(rp));
  ps.origin[0]=31;ps.origin[1]=-17;ps.origin[2]=30;
  ps.velocity[0]=(n&256)?100:0;ps.velocity[1]=(n&512)?-100:0;ps.velocity[2]=10;
  ps.clientNum=13;move.ps=&ps;move.tracemask=0x2010001;move.trace=collision;
  VectorSet(move.mins,-16,-16,-30);VectorSet(move.maxs,16,16,57);pm=&move;
  memcpy(raw,&ps,0x420);*(void **)rp=raw;*(int *)(rp+0x44)=move.tracemask;
  memcpy(rp+0xf4,move.mins,12);memcpy(rp+0x100,move.maxs,12);*(void **)(rp+(server?0x128:0x124))=collision;
  *(void **)(base+(server?0x23cf4fc:0x4ba9edc))=rp;active=(playerState_t *)raw;reset();original((n>>1)&1);
  et=traces;es=slides;ee=events;ev=eventValue;eg=gravityValue;memcpy(expected,vectors,sizeof(vectors));memcpy(ea,args,sizeof(args));
  active=&ps;reset();PM_StepSlideMove((n>>1)&1);
  /* x87 keeps the multiply/subtract in extended precision; tolerate its
   * sub-micro-unit residual in the six movement floats only. */
  {int j;for(j=0;j<6;++j)if(fabs(((float *)(raw+0x14))[j]-((float *)ps.origin)[j])<0.00001f)
    memcpy(raw+0x14+j*4,(unsigned char *)ps.origin+j*4,4);}
  if(memcmp(raw,&ps,0x420)||et!=traces||es!=slides||ee!=events||ev!=eventValue||eg!=gravityValue||memcmp(expected,vectors,sizeof(vectors))||memcmp(ea,args,sizeof(args))) {
   int j;fprintf(stderr,"step mismatch scenario %d traces %d/%d slides %d/%d events %d/%d values %d/%d\n",n,et,traces,es,slides,ee,events,ev,eventValue);
   for(j=0;j<0x420;++j)if(raw[j]!=((unsigned char *)&ps)[j])fprintf(stderr,"ps byte %x: %02x/%02x\n",j,raw[j],((unsigned char *)&ps)[j]);
   for(j=0;j<72;++j)if(((float *)expected)[j]!=((float *)vectors)[j])fprintf(stderr,"trace float %d: %g/%g\n",j,((float *)expected)[j],((float *)vectors)[j]);return 1;
  }
 }
 printf("%d production step-controller cases match original DLL\n",n);FreeLibrary(dll);return 0;
}
