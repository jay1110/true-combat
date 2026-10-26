#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_draw.c"
#include <windows.h>
#include <stdio.h>
static int n,cullResult,pvsResult,normCall;static byte events[64][256];static float distanceValue;
static float norm(vec3_t v){int*p=(int*)events[n++];p[0]=1;memcpy(p+1,v,12);normCall++;v[0]=.6f;v[1]=.8f;v[2]=0;return distanceValue;}
static int cull(vec3_t p,float radius){int*q=(int*)events[n++];q[0]=2;memcpy(q+1,p,12);memcpy(q+4,&radius,4);return cullResult;}
static qboolean pvs(const vec3_t a,const vec3_t b){int*p=(int*)events[n++];p[0]=3;memcpy(p+1,a,12);memcpy(p+4,b,12);return pvsResult;}
static void entity(const refEntity_t*r){*(int*)events[n]=4;memcpy(events[n++]+4,r,sizeof(*r));}
static void poly(qhandle_t shader,int count,const polyVert_t*v){int*p=(int*)events[n++];p[0]=5;p[1]=shader;p[2]=count;memcpy(p+3,v,count*sizeof(*v));}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE m;byte*b;void(__cdecl*f[2])(void);int k,op,i,j,count,normalizations;byte copy[64][256];refdef_t view;
 if(argc==2&&!strcmp(argv[1],"--offsets")){printf("{\"cg_numMiscGameModels\":%u,\"cg_numMiscClientSprites\":%u,\"cgs_miscGameModels\":%u,\"cgs_miscClientSprites\":%u,\"original_modelCount\":\"34081fa0\",\"original_spriteCount\":\"34846e64\",\"original_models\":\"33fc9338\",\"original_sprites\":\"33fdf3d4\"}\n",(unsigned)offsetof(cg_t,numMiscGameModels),(unsigned)offsetof(cg_t,numMiscClientSprites),(unsigned)offsetof(cgs_t,miscGameModels),(unsigned)offsetof(cgs_t,miscClientSprites));return 0;}
 if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;b=(byte*)m;f[0]=(void*)(b+0x21da0);f[1]=(void*)(b+0x22020);if(sizeof(refEntity_t)!=236||sizeof(cg_gamemodel_t)!=84||sizeof(cg_clientsprite_t)!=48)return 3;
#define H(off,fn,to) hook(b+off,to);hook(fn,to)
 H(0x7e520,VectorNormalize,norm);H(0x6b5c0,CG_CullPointAndRadius,cull);H(0x65ae0,trap_R_inPVS,pvs);H(0x65290,trap_R_AddRefEntityToScene,entity);H(0x652b0,trap_R_AddPolyToScene,poly);
 memset(&view,0,sizeof(view));cg.refdef_current=&view;*(void**)(b+0x40863ac)=&view;
 for(k=0;k<8192;k++)for(op=0;op<2;op++){
  cg.numMiscGameModels=*(int*)(b+0x4081fa0)=k%5;cg.numMiscClientSprites=*(int*)(b+0x4846e64)=k%5;cullResult=(k/5)%2;pvsResult=(k/10)%2;distanceValue=(k%131)*1.13f;view.vieworg[0]=-1.3f;view.vieworg[1]=5.7f;view.vieworg[2]=2.9f;view.viewaxis[0][0]=(k/20)%2?1:0;view.viewaxis[0][1]=0;
  for(i=0;i<4;i++){cg_gamemodel_t*g=&cgs.miscGameModels[i];cg_clientsprite_t*s=&cgs.miscClientSprites[i];memset(g,0,sizeof(*g));memset(s,0,sizeof(*s));g->model=77+i;s->shader=k%7?88+i:0;g->radius=(k/40+i)%2?3.7f:0;g->billboard=(k/80+i)%2;g->billboardScale=1.17f;g->drawDistance=s->drawDistance=100+(k%3)*10;g->fadeDistance=s->fadeDistance=k%4==0?120:53;for(j=0;j<3;j++){g->org[j]=s->org[j]=i*.37f+j*1.79f;g->color[j]=s->color[j]=(j+1)*.231f;g->axes[j][j]=1.31f;}s->halfWidth=3.71f;s->top=4.31f;s->bottom=-1.37f;}
  memcpy(b+0x3fc9338,cgs.miscGameModels,4*84);memcpy(b+0x3fdf3d4,cgs.miscClientSprites,4*48);
  n=normCall=0;memset(events,0,sizeof(events));f[op]();count=n;normalizations=normCall;memcpy(copy,events,sizeof(copy));n=normCall=0;memset(events,0,sizeof(events));if(op)CG_DrawMiscClientsprites();else CG_DrawMiscGamemodels();
  if(count!=n||normalizations!=normCall||memcmp(copy,events,sizeof(copy))){printf("misc renderer case%d op%d events%d/%d\n",k,op,count,n);for(i=0;i<sizeof(copy);i++)if(((byte*)copy)[i]!=((byte*)events)[i]){printf("byte%d expected%02x actual%02x\n",i,((byte*)copy)[i],((byte*)events)[i]);break;}return 1;}
 }puts("16384 whole misc renderer comparisons passed (8192 each)");return 0;}
