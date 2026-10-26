#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/tce_wall_renderer.h"
#include "../game/tce_bg.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
typedef struct {int type,i[12];float f[24];char text[80];} event_t;
static event_t events[256],expected[256];static int n,scenario,testCase,randCalls;static unsigned rng;
static byte *referenceBase;static const char *activeStage="setup";
static LONG WINAPI wallException(EXCEPTION_POINTERS *info){
 fprintf(stderr,"wall exception case%d scenario%d stage%s code%08lx address%p originalBase%p accessed%p\n",testCase,scenario,activeStage,info->ExceptionRecord->ExceptionCode,info->ExceptionRecord->ExceptionAddress,referenceBase,(void*)(info->ExceptionRecord->NumberParameters>1?info->ExceptionRecord->ExceptionInformation[1]:0));
 fflush(stderr);return EXCEPTION_EXECUTE_HANDLER;
}
static void require(int b,const char *s){if(!b){printf("wall case %d scenario %d: %s\n",testCase,scenario,s);exit(1);}}
static event_t *event(int kind){event_t*e=&events[n++];require(n<256,"overflow");memset(e,0,sizeof(*e));e->type=kind;return e;}
static void v(float *out,const float *in){if(in)memcpy(out,in,12);else out[0]=-9876;}
int rand(void){rng=rng*1664525u+1013904223u;++randCalls;return(rng>>16)&32767;}
static void hook(void *at,void *fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static int realSurface(int t){return (int)BG_SurfaceType2Flag((unsigned)t);}
static int surface(int t){event(1)->i[0]=t;return t<<24;}
static int volume(vec3_t p,float a,float b,int c){event_t*e=event(2);v(e->f,p);e->f[3]=a;e->f[4]=b;e->i[0]=c;return scenario%3?93:0;}
static void print(const char *s){strncpy(event(3)->text,s,79);}
static void particles(int kind,vec3_t p,vec3_t d,int a,int b,int c,float f){event_t*e=event(kind);v(e->f,p);v(e->f+3,d);e->i[0]=a;e->i[1]=b;e->i[2]=c;e->f[6]=f;}
static void bullet(vec3_t p,vec3_t d,int a,int b,int c,float f){particles(4,p,d,a,b,c,f);}
static void dirt(vec3_t p,vec3_t d,int a,int b,int c,float f,float g,float h,float j,int k){event_t*e;particles(5,p,d,a,b,c,f);e=&events[n-1];e->f[7]=g;e->f[8]=h;e->f[9]=j;e->i[3]=k;}
static void particle(vec3_t p,vec3_t d,int a,int b,float f){particles(6,p,d,a,b,0,f);VectorNormalize(d);}
static void flat(vec3_t p,vec3_t d,int a){particles(7,p,d,a,0,0,0);}
static void sparks(vec3_t p,vec3_t d,int a){particles(8,p,d,a,0,0,0);}
static void explode(vec3_t p,vec3_t d,int a,int b,int c,int f,int g){event_t*e;particles(9,p,d,a,b,c,0);e=&events[n-1];e->i[3]=f;e->i[4]=g;}
static void debrisParticles(vec3_t p,vec3_t d,int a,int b,int c,float f){particles(10,p,d,a,b,c,f);}
static void concussion(vec3_t p,int a){event_t*e=event(11);v(e->f,p);e->i[0]=a;}
static void *smoke(vec3_t p,vec3_t d,float a,float b,float c,float f,float g,float h,int j,int k,int l,int m){event_t*e=event(12);v(e->f,p);v(e->f+3,d);e->f[6]=a;e->f[7]=b;e->f[8]=c;e->f[9]=f;e->f[10]=g;e->f[11]=h;e->i[0]=j;e->i[1]=k;e->i[2]=l;e->i[3]=m;return NULL;}
static int contents(const vec3_t p,int a){event_t*e=event(13);v(e->f,p);e->i[0]=a;return scenario%2?32:0;}
static int cmContents(const vec3_t p,int a){event_t*e=event(14);v(e->f,p);e->i[0]=a;return scenario%2?32:0;}
static void trace(trace_t*out,const vec3_t s,const vec3_t end,const vec3_t mi,const vec3_t ma,int a,int mask){event_t*e=event(15);v(e->f,s);v(e->f+3,end);v(e->f+6,mi);v(e->f+9,ma);e->i[0]=a;e->i[1]=mask;memset(out,0,sizeof(*out));out->fraction=.4f;VectorSet(out->endpos,3,5,7);VectorSet(out->plane.normal,.25f,.5f,.75f);out->surfaceFlags=((scenario/2)%34)<<24;out->entityNum=1022;}
static void explosion(const char *s,vec3_t p,vec3_t d,int a,int b,int c,int f){event_t*e;particles(16,p,d,a,b,c,0);e=&events[n-1];e->i[3]=f;strncpy(e->text,s,79);}
static void debris(vec3_t p,vec3_t d,int a,int b,int c){particles(17,p,d,a,b,c,0);}
static void ripple(int s,vec3_t p,vec3_t d,int a,int b){particles(18,p,d,s,a,b,0);}
static void elite(int s,vec3_t p,vec3_t d,float a,float r,float g,float b,float f,int fade,float radius,int temporary,int duration){event_t*e=event(19);v(e->f,p);v(e->f+3,d);e->i[0]=s;e->i[1]=fade;e->i[2]=temporary;e->i[3]=duration;e->f[6]=a;e->f[7]=r;e->f[8]=g;e->f[9]=b;e->f[10]=f;e->f[11]=radius;}
static void impact(int s,vec3_t p,vec3_t d,float radius,float angle,float r,float g,float b,float a,int duration){event_t*e=event(20);v(e->f,p);v(e->f+3,d);e->i[0]=s;e->i[1]=duration;e->f[6]=radius;e->f[7]=angle;e->f[8]=r;e->f[9]=g;e->f[10]=b;e->f[11]=a;}
static int shader(const char*s){strncpy(event(21)->text,s,79);return 1234;}
static void sound(const vec3_t p,int a,int b,int c,int d){event_t*e=event(22);v(e->f,p);e->i[0]=a;e->i[1]=b;e->i[2]=c;e->i[3]=d;}
static void soundEx(const vec3_t p,int a,int b,int c,int d,int f){event_t*e=event(23);v(e->f,p);e->i[0]=a;e->i[1]=b;e->i[2]=c;e->i[3]=d;e->i[4]=f;}
static void decal(int s,int n,const vec3_t p,const vec4_t projection,const vec4_t color,int duration,int fade){event_t*e=event(24);v(e->f,p);memcpy(e->f+3,projection,16);memcpy(e->f+7,color,16);e->i[0]=s;e->i[1]=n;e->i[2]=duration;e->i[3]=fade;}
int main(int argc,char **argv){HMODULE dll;byte*b;int k,j,i,en,erc;unsigned erng;refdef_t view;tce_wallRendererContext_t ctx;vec3_t p={2000,400,700},d,ed,dir={1,2,3};
 void(__cdecl*original)(int,int,vec3_t,vec3_t,vec3_t,unsigned,int,int);
 SetUnhandledExceptionFilter(wallException);
 if((argc!=2&&argc!=3)||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;referenceBase=b;original=(void*)(b+0x78260);
 memset(&ctx,0,sizeof(ctx));memset(&view,0,sizeof(view));ctx.view=(refdef_t *const *)(b+0x40863ac);
#define BIND(field,address,callback) ctx.field=callback;hook(b+(address),callback)
 if(argc==3){ctx.surfaceType=realSurface;}else{BIND(surfaceType,0x3b50,surface);}BIND(soundVolume,0x44b50,volume);BIND(print,0x47b10,print);
 BIND(bulletParticles,0x77100,bullet);BIND(dirtParticles,0x77d50,dirt);BIND(particle,0x2a870,particle);BIND(flatSparks,0x2b730,flat);BIND(sparks,0x2b9a0,sparks);BIND(explode,0x367c0,explode);BIND(debrisParticles,0x77330,debrisParticles);BIND(concussion,0x774f0,concussion);BIND(smokePuff,0x2b570,smoke);BIND(pointContents,0x587d0,contents);BIND(cmPointContents,0x64e20,cmContents);BIND(boxTrace,0x64e60,trace);BIND(particleExplosion,0x51f70,explosion);BIND(debris,0x77eb0,debris);BIND(ripple,0x781a0,ripple);BIND(eliteMark,0x4bbd0,elite);BIND(impactMark,0x4b9c0,impact);BIND(registerShader,0x65a30,shader);BIND(sound,0x64fe0,sound);BIND(soundEx,0x65040,soundEx);BIND(projectDecal,0x64f70,decal);hook(b+0x885b6,rand);
#undef BIND
 ctx.media.explosionFlash=(const int*)(b+0x2588280);*(int*)(b+0x2588280)=100;
 ctx.media.waterRipple=(const int*)(b+0x258843c);*(int*)(b+0x258843c)=101;
 ctx.media.bulletDefault=(const int*)(b+0x2588468);*(int*)(b+0x2588468)=102;
 ctx.media.bulletMetal=(const int*)(b+0x258846c);*(int*)(b+0x258846c)=103;
 ctx.media.bulletWood=(const int*)(b+0x2588470);*(int*)(b+0x2588470)=104;
 ctx.media.bulletGlass=(const int*)(b+0x2588474);*(int*)(b+0x2588474)=105;
 ctx.media.burn=(const int*)(b+0x2588478);*(int*)(b+0x2588478)=106;
 ctx.media.bulletStone=(const int*)(b+0x258847c);*(int*)(b+0x258847c)=107;
 ctx.media.knife=(const int*)(b+0x2588480);*(int*)(b+0x2588480)=108;
 ctx.media.exitStone=(const int*)(b+0x2588488);*(int*)(b+0x2588488)=109;
 ctx.media.exitMetal=(const int*)(b+0x258848c);*(int*)(b+0x258848c)=110;
 ctx.media.exitWood=(const int*)(b+0x2588494);*(int*)(b+0x2588494)=111;
 ctx.media.dirt=(const int*)(b+0x2588578);*(int*)(b+0x2588578)=112;
 ctx.media.water=(const int*)(b+0x258857c);*(int*)(b+0x258857c)=113;
 ctx.media.snow=(const int*)(b+0x2588580);*(int*)(b+0x2588580)=114;
 ctx.media.grass=(const int*)(b+0x2588590);*(int*)(b+0x2588590)=115;
 ctx.media.mud=(const int*)(b+0x2588594);*(int*)(b+0x2588594)=116;
 ctx.media.rocket[0]=*(int*)(b+0x25886d4)=117;
 ctx.media.rocket[1]=*(int*)(b+0x25886d8)=118;
 ctx.media.rocket[2]=*(int*)(b+0x25886dc)=119;
 ctx.media.rocketFar=*(int*)(b+0x25886e0)=120;
 ctx.media.mortar[0]=*(int*)(b+0x25886e4)=121;
 ctx.media.mortar[1]=*(int*)(b+0x25886e8)=122;
 ctx.media.mortar[2]=*(int*)(b+0x25886ec)=123;
 ctx.media.mortarFar=*(int*)(b+0x25886f0)=124;
 ctx.media.explosion=(const int*)(b+0x2588870);*(int*)(b+0x2588870)=125;
 ctx.media.explosionFar=(const int*)(b+0x2588874);*(int*)(b+0x2588874)=126;
 ctx.media.explosionWater=(const int*)(b+0x2588878);*(int*)(b+0x2588878)=127;
 ctx.media.dynamite=*(int*)(b+0x258887c)=128;
 ctx.media.dynamiteFar=*(int*)(b+0x2588880)=129;
 ctx.media.satchel=*(int*)(b+0x2588884)=130;
 ctx.media.satchelFar=*(int*)(b+0x2588888)=131;
 ctx.media.grenade=(const int*)(b+0x25888a0);*(int*)(b+0x25888a0)=132;
 ctx.media.grenadeFar=(const int*)(b+0x25888a4);*(int*)(b+0x25888a4)=133;
 ctx.media.fabricHit[0]=(const int*)(b+0x2588ac8);*(int*)(b+0x2588ac8)=134;
 ctx.media.fabricHit[1]=(const int*)(b+0x2588acc);*(int*)(b+0x2588acc)=135;
 ctx.media.fabricHit[2]=(const int*)(b+0x2588ad0);*(int*)(b+0x2588ad0)=136;
 ctx.media.fabricHit[3]=(const int*)(b+0x2588ad4);*(int*)(b+0x2588ad4)=137;
 ctx.media.fabricHit[4]=(const int*)(b+0x2588ad8);*(int*)(b+0x2588ad8)=138;
 ctx.media.metal2Hit[0]=(const int*)(b+0x2588adc);*(int*)(b+0x2588adc)=139;
 ctx.media.metal2Hit[1]=(const int*)(b+0x2588ae0);*(int*)(b+0x2588ae0)=140;
 ctx.media.metal2Hit[2]=(const int*)(b+0x2588ae4);*(int*)(b+0x2588ae4)=141;
 ctx.media.metal2Hit[3]=(const int*)(b+0x2588ae8);*(int*)(b+0x2588ae8)=142;
 ctx.media.metal2Hit[4]=(const int*)(b+0x2588aec);*(int*)(b+0x2588aec)=143;
 ctx.media.strawHit[0]=(const int*)(b+0x2588b40);*(int*)(b+0x2588b40)=144;
 ctx.media.strawHit[1]=(const int*)(b+0x2588b44);*(int*)(b+0x2588b44)=145;
 ctx.media.strawHit[2]=(const int*)(b+0x2588b48);*(int*)(b+0x2588b48)=146;
 ctx.media.strawHit[3]=(const int*)(b+0x2588b4c);*(int*)(b+0x2588b4c)=147;
 ctx.media.strawHit[4]=(const int*)(b+0x2588b50);*(int*)(b+0x2588b50)=148;
 ctx.media.explosive4=(const int*)(b+0x2588b54);*(int*)(b+0x2588b54)=149;
 ctx.media.explosive30=(const int*)(b+0x2588b58);*(int*)(b+0x2588b58)=150;
 ctx.media.metalHit[0]=(const int*)(b+0x2588b98);*(int*)(b+0x2588b98)=151;
 ctx.media.metalHit[1]=(const int*)(b+0x2588b9c);*(int*)(b+0x2588b9c)=152;
 ctx.media.metalHit[2]=(const int*)(b+0x2588ba0);*(int*)(b+0x2588ba0)=153;
 ctx.media.metalHit[3]=(const int*)(b+0x2588ba4);*(int*)(b+0x2588ba4)=154;
 ctx.media.metalHit[4]=(const int*)(b+0x2588ba8);*(int*)(b+0x2588ba8)=155;
 ctx.media.woodHit[0]=(const int*)(b+0x2588bac);*(int*)(b+0x2588bac)=156;
 ctx.media.woodHit[1]=(const int*)(b+0x2588bb0);*(int*)(b+0x2588bb0)=157;
 ctx.media.woodHit[2]=(const int*)(b+0x2588bb4);*(int*)(b+0x2588bb4)=158;
 ctx.media.woodHit[3]=(const int*)(b+0x2588bb8);*(int*)(b+0x2588bb8)=159;
 ctx.media.woodHit[4]=(const int*)(b+0x2588bbc);*(int*)(b+0x2588bbc)=160;
 ctx.media.glassHit[0]=(const int*)(b+0x2588bc0);*(int*)(b+0x2588bc0)=161;
 ctx.media.glassHit[1]=(const int*)(b+0x2588bc4);*(int*)(b+0x2588bc4)=162;
 ctx.media.glassHit[2]=(const int*)(b+0x2588bc8);*(int*)(b+0x2588bc8)=163;
 ctx.media.glassHit[3]=(const int*)(b+0x2588bcc);*(int*)(b+0x2588bcc)=164;
 ctx.media.glassHit[4]=(const int*)(b+0x2588bd0);*(int*)(b+0x2588bd0)=165;
 ctx.media.stoneHit[0]=(const int*)(b+0x2588bd4);*(int*)(b+0x2588bd4)=166;
 ctx.media.stoneHit[1]=(const int*)(b+0x2588bd8);*(int*)(b+0x2588bd8)=167;
 ctx.media.stoneHit[2]=(const int*)(b+0x2588bdc);*(int*)(b+0x2588bdc)=168;
 ctx.media.stoneHit[3]=(const int*)(b+0x2588be0);*(int*)(b+0x2588be0)=169;
 ctx.media.stoneHit[4]=(const int*)(b+0x2588be4);*(int*)(b+0x2588be4)=170;
 ctx.media.artillery=(const int*)(b+0x2588bfc);*(int*)(b+0x2588bfc)=171;
 ctx.media.artilleryFar=(const int*)(b+0x2588c00);*(int*)(b+0x2588c00)=172;
 ctx.media.knifeFlesh[0]=(const int*)(b+0x2588c08);*(int*)(b+0x2588c08)=173;
 ctx.media.knifeFlesh[1]=(const int*)(b+0x2588c0c);*(int*)(b+0x2588c0c)=174;
 ctx.media.knifeFlesh[2]=(const int*)(b+0x2588c10);*(int*)(b+0x2588c10)=175;
 ctx.media.knifeFlesh[3]=(const int*)(b+0x2588c14);*(int*)(b+0x2588c14)=176;
 ctx.media.knifeWall=(const int*)(b+0x2588c18);*(int*)(b+0x2588c18)=177;
 *(void**)(b+0x40863ac)=&view;VectorSet(view.vieworg,10,20,30);view.viewaxis[0][0]=1;
 for(k=0;k<124096;++k){int weapon=k%66,material=(k/66)%34,effect=(k/(66*34))%4,type=(k/(66*34*4))%2,force=(k/(66*34*8))%2;unsigned flags;scenario=(k/66)%68;testCase=k;
  ctx.time=(const int*)(b+0x407dea4);*(int*)(b+0x407dea4)=10000+k;ctx.markTime=(const int*)(b+0x4012d4c);*(int*)(b+0x4012d4c)=12345;ctx.forceMarks=(const qboolean*)(b+0x40a2424);*(int*)(b+0x40a2424)=scenario%2;ctx.portal=(const int*)(b+0x40a4a1c);*(int*)(b+0x40a4a1c)=scenario%3==0;
  view.fov_x=scenario%3==1?20:90;ctx.weaponMaterialMode[weapon]=(const int*)(b+0x4c1e068+weapon*0x1cc);*(int*)(b+0x4c1e068+weapon*0x1cc)=scenario%2;
  VectorSet(d,-.25f,.5f,.75f);flags=type||weapon==1?material:material<<24;
  if(k>=107712){float distances[]={0,100,1199,1200,1201,7999,8000,8001,9000};VectorSet(p,distances[(k/16)%9],0,0);VectorClear(view.vieworg);view.fov_x=k%3==0?29.99f:k%3==1?30.f:90.f;d[0]=k%2?.25f:-.25f;if(k%7==0&&!type&&weapon!=1)flags|=4;ctx.forceMarks=(const qboolean*)(b+0x40a2424);*(int*)(b+0x40a2424)=k%2;}
  VectorCopy(d,ed);n=randCalls=0;rng=k;
  activeStage="original";original(weapon,effect,p,ed,dir,flags,force,type);en=n;erc=randCalls;erng=rng;memcpy(expected,events,sizeof(expected));
  n=randCalls=0;rng=k;activeStage="port";TCE_CG_MissileHitWall(weapon,effect,p,d,dir,flags,force,type,&ctx);
  if(n!=en||randCalls!=erc||rng!=erng){printf("count%d/%d RNG%d/%d\n",n,en,randCalls,erc);require(0,"event or RNG count");}
  for(i=0;i<n;++i){if(events[i].type!=expected[i].type||memcmp(events[i].i,expected[i].i,sizeof(events[i].i))||strcmp(events[i].text,expected[i].text)){printf("event%d kind%d/%d text%s/%s\n",i,events[i].type,expected[i].type,events[i].text,expected[i].text);for(j=0;j<12;++j)printf("arg%d %d/%d\n",j,events[i].i[j],expected[i].i[j]);require(0,"arguments");}
   for(j=0;j<24;++j)if(!_finite(events[i].f[j])||!_finite(expected[i].f[j])||fabs(events[i].f[j]-expected[i].f[j])>.002+fabs(expected[i].f[j])*.000004){printf("event%d kind%d float%d %.9g/%.9g\n",i,events[i].type,j,events[i].f[j],expected[i].f[j]);require(0,"float values");}}
  for(j=0;j<3;++j)require(fabs(d[j]-ed[j])<.00001,"normal mutation");
 }
 printf("124096 complete wall-controller original comparisons passed (%s surface decoder)\n",argc==3?"real":"stubbed");return 0;
}
#undef _vsnprintf
int QDECL Q_vsnprintf(char*d,int n,const char*f,va_list a){return _vsnprintf(d,n,f,a);}
void QDECL Com_Printf(const char*f,...){(void)f;}
void QDECL Com_Error(int level,const char*f,...){(void)level;(void)f;abort();}
