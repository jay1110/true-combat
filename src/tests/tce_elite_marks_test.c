#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
extern markPoly_t cg_activeMarkPolys, *cg_freeMarkPolys;
extern markPoly_t *CG_AllocMark(int);
extern void CG_FreeMarkPoly(markPoly_t *);
static int scenario, cmCalls, polyCalls, errorCalls, current;
static jmp_buf onError;
typedef struct { int angle, maxPoints, maxFragments; vec3_t points[4], projection; } cmRecord;
typedef struct {int shader,n;polyVert_t v[10];} polyRecord;
static cmRecord cm[2],savedCm[2];
static polyRecord polys[256],savedPolys[256];
static markPoly_t expected[256];
static int expectedCount,savedError,savedCM,savedPoly;
static int decalCalls,dc[4],savedDc[4];
static float df[20],savedDf[20];
static void decal(int shader,int n,const vec3_t *p,const vec4_t projection,const vec4_t color,int lifetime,int fade){
 ++decalCalls;dc[0]=shader;dc[1]=n;dc[2]=lifetime;dc[3]=fade;
 memcpy(df,p,48);memcpy(df+12,projection,16);memcpy(df+16,color,16);
}
static void hook(void *at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static void fail(const char*s){printf("case%d %s\n",current,s);exit(1);}
static void error(const char *fmt,...){++errorCalls;longjmp(onError,1);}
static int fragments(int angle,const vec3_t *p,const vec3_t projection,int maxPoints,vec3_t out,int maxFragments,markFragment_t *f){
 int i,j,n=3; float *v=out; cmRecord *r=&cm[cmCalls++];if(cmCalls>2)fail("cm overflow");
 r->angle=angle;r->maxPoints=maxPoints;r->maxFragments=maxFragments;memcpy(r->points,p,sizeof(r->points));VectorCopy(projection,r->projection);
 if(scenario%5==0 && cmCalls==1)return 0;
 if(scenario%7==0)return 0;
 for(i=0;i<n;++i){f[i].firstPoint=i*12;f[i].numPoints=i==0?3:(i==1?-4:12);}
 for(i=0;i<36;++i)for(j=0;j<5;++j)v[i*5+j]=(float)(i*7+j-13)*.13f;
 return n;
}
static void addPoly(int shader,int n,const polyVert_t*v){polyRecord*r=&polys[polyCalls++];if(polyCalls>256||n>10)fail("poly bounds");r->shader=shader;r->n=n;memcpy(r->v,v,n*sizeof(*v));}
static void reset(void){cmCalls=polyCalls=errorCalls=0;memset(cm,0,sizeof(cm));memset(polys,0,sizeof(polys));}
static void sameFloat(float a,float b){if(!_finite(a)||!_finite(b)||fabs(a-b)>.00008+fabs(b)*.000003){printf("%.9g / %.9g ",a,b);fail("float");}}
static void verts(polyVert_t*a,polyVert_t*b,int n){int i,j;for(i=0;i<n;++i){for(j=0;j<3;++j)sameFloat(a[i].xyz[j],b[i].xyz[j]);for(j=0;j<2;++j)sameFloat(a[i].st[j],b[i].st[j]);if(memcmp(a[i].modulate,b[i].modulate,4))fail("color");}}
static int saveMarks(markPoly_t*head,markPoly_t*dest){markPoly_t*m;int n=0;for(m=head->nextMark;m!=head;m=m->nextMark){if(n==256)fail("list loop");dest[n++]=*m;}return n;}
static void compareMarks(markPoly_t*head){markPoly_t*m;int n=0;for(m=head->nextMark;m!=head;m=m->nextMark){markPoly_t *e=&expected[n++];if(n>expectedCount)fail("mark count");if(m->time!=e->time||m->markShader!=e->markShader||m->alphaFade!=e->alphaFade||m->duration!=e->duration||m->poly.numVerts!=e->poly.numVerts||memcmp(m->color,e->color,sizeof(m->color)))fail("mark fields");verts(m->verts,e->verts,m->poly.numVerts);}if(n!=expectedCount)fail("mark count");}
static void save(void){savedCM=cmCalls;savedPoly=polyCalls;savedError=errorCalls;memcpy(savedCm,cm,sizeof(cm));memcpy(savedPolys,polys,sizeof(polys));}
static void compare(void){int i,j;if(cmCalls!=savedCM||polyCalls!=savedPoly||errorCalls!=savedError)fail("callbacks");for(i=0;i<cmCalls;++i){if(cm[i].angle!=savedCm[i].angle||cm[i].maxPoints!=savedCm[i].maxPoints||cm[i].maxFragments!=savedCm[i].maxFragments)fail("fragment arguments");for(j=0;j<12;++j)sameFloat(((float*)cm[i].points)[j],((float*)savedCm[i].points)[j]);for(j=0;j<3;++j)sameFloat(cm[i].projection[j],savedCm[i].projection[j]);}for(i=0;i<polyCalls;++i){if(polys[i].shader!=savedPolys[i].shader||polys[i].n!=savedPolys[i].n)fail("poly arguments");verts(polys[i].v,savedPolys[i].v,polys[i].n);}}
int main(int argc,char**argv){byte*b;HMODULE dll;int k,phase;markPoly_t*head;void(__cdecl*init)(void);void(__cdecl*draw)(void);markPoly_t*(__cdecl*alloc)(int);void(__cdecl*freeMark)(markPoly_t*);void(__cdecl*elite)(int,float*,float*,float,float,float,float,float,int,float,int,int);
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;if(sizeof(markPoly_t)!=0x124)return 3;head=(void*)(b+0x25636e0);init=(void*)(b+0x4b8c0);draw=(void*)(b+0x4c160);alloc=(void*)(b+0x4b950);freeMark=(void*)(b+0x4b910);elite=(void*)(b+0x4bbd0);
 hook(b+0x64f40,fragments);hook(trap_CM_MarkFragments,fragments);hook(b+0x652b0,addPoly);hook(trap_R_AddPolyToScene,addPoly);hook(b+0x47bf0,error);hook(CG_Error,error);
 for(k=0;k<20000;++k){vec3_t origin={2.7f,-3.3f,5.1f},dir={(k%5)-2.f,(k%7)-3.f,(k%11)-5.f};float radius=k%101==0?-1:(k%13+1)*.17f,angle=(k%721-360)*.37f;int duration=k%3==0?-2:(k%3==1?-1:2000);if(DotProduct(dir,dir)==0)dir[2]=1;current=scenario=k;cg.time=*(int*)(b+0x407dea4)=10000;cg_markTime.integer=*(int*)(b+0x4012d4c)=k%17==0?0:4000;
 init();reset();if(!setjmp(onError))elite(31,origin,dir,angle,.17f,.39f,.83f,.71f,k%2,radius,(k/2)%2,duration);save();expectedCount=saveMarks(head,expected);
 CG_InitMarkPolys();reset();if(!setjmp(onError))CG_EliteImpactMark(31,origin,dir,angle,.17f,.39f,.83f,.71f,k%2,radius,(k/2)%2,duration);compare();compareMarks(&cg_activeMarkPolys);
 for(phase=0;phase<4;++phase){cg.time=*(int*)(b+0x407dea4)=11000+phase*1000;reset();draw();save();expectedCount=saveMarks(head,expected);reset();CG_AddMarks();compare();compareMarks(&cg_activeMarkPolys);}
 }
 /* Exercise real pool saturation/recycling and explicit Free, no allocator stub. */
 init();CG_InitMarkPolys();for(k=0;k<1100;++k){markPoly_t*x=alloc(1),*y=CG_AllocMark(1);x->time=y->time=1+k/3;current=k;expectedCount=saveMarks(head,expected);compareMarks(&cg_activeMarkPolys);if(k%7==0){freeMark(x);CG_FreeMarkPoly(y);expectedCount=saveMarks(head,expected);compareMarks(&cg_activeMarkPolys);}}
 hook(b+0x64f70,decal);hook(trap_R_ProjectDecal,decal);
 for(k=0;k<30000;++k){vec3_t origin={-4.7f,8.2f,13.9f};vec4_t projection={(k%5)-2.f,(k%7)-3.f,(k%11)-5.f,8.f};int life=k%9==0?0:(k%9==1?-1:1000+k),n,j;float radius=(k%33-4)*.27f,angle=(k%721-360)*.73f;void(__cdecl*impact)(int,float*,float*,float,float,float,float,float,float,int)=(void*)(b+0x4b9c0);
 current=100000+k;if(projection[0]==0&&projection[1]==0&&projection[2]==0)projection[2]=1;
 decalCalls=0;impact(41,origin,projection,radius,angle,.13f,.27f,.59f,.71f,life);n=decalCalls;memcpy(savedDc,dc,sizeof(dc));memcpy(savedDf,df,sizeof(df));
 decalCalls=0;CG_ImpactMark(41,origin,projection,radius,angle,.13f,.27f,.59f,.71f,life);if(decalCalls!=n)fail("decal count");if(n){if(memcmp(dc,savedDc,sizeof(dc)))fail("decal args");for(j=0;j<20;++j)sameFloat(df[j],savedDf[j]);}
 }
 printf("30000 ImpactMark,20000 EliteImpactMark,80000 AddMarks states,1100 pool lifecycle comparisons passed\n");return 0;
}
