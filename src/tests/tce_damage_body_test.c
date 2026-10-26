#define WIN32_LEAN_AND_MEAN
#include "../game/g_local.h"
#include "../game/tce_damage.h"
#include <windows.h>
#include <stdio.h>
static unsigned char *base,*entity,*part,client[0x1700];
static tceDamageVolume_t captured[3];static float fractions[3];static int calls;
static void patch(void *at,void *fn){DWORD o;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);}
static void *spawn(void){memset(part,0,0x57c);*(int *)part=70;return part;}
static void nop(void *p,...){(void)p;}
static int sameTeam(void *a,void *b){return a==b;}
static void trace(trace_t *tr,const float *start,const float *mins,const float *maxs,const float *end,int pass,int mask){
 int n=calls++;if(n>2)exit(9);memcpy(captured[n].origin,part+0x168,12);memcpy(captured[n].mins,part+0x134,12);memcpy(captured[n].maxs,part+0x140,12);
 memset(tr,0,sizeof(*tr));tr->fraction=fractions[n];tr->entityNum=fractions[n]<1?70:1022;memcpy(tr->endpos,end,12);
}
static void setup(const tceDamagePose_t *pose){
 memset(entity,0,0x57c);memset(client,0,sizeof(client));*(int *)(entity+0x194)=1;
 *(int *)(entity+0x2c8)=100;*(int *)(entity+0x2cc)=1;*(void **)(entity+0x190)=client;*(void **)(entity+0x2b8)=nop;
 *(int *)(client+0xb30)=1;*(int *)(client+0xa4)=pose->weapon;*(int *)(client+0xbc)=40;*(int *)(client+0xd0)=100;
 *(int *)(client+0x1334)=-10000;*(int *)(client+0x68)=pose->prone?EF_PRONE:0;*(int *)(client+0xc)=pose->ducked?PMF_DUCKED:0;
 *(int *)(client+0xf0)=pose->aiming?0x20:0;*(int *)(client+0xf4)=pose->enlarged?0x200:0;
 *(float *)(client+0x3c)=pose->lean;memcpy(client+0xb0,pose->angles,12);memcpy(entity+0x168,pose->origin,12);
 *(int *)(base+0x7febe0)=10000;*(int *)(base+0x7ffc5c)=0;*(int *)(base+0x784f6c)=0;
 *(int *)(base+0x7852cc)=0;*(int *)(base+0x7849cc)=0;*(int *)(base+0x6d8d4c)=0;calls=0;
}
int main(int argc,char **argv){
 HMODULE dll;float (__cdecl *damage)(void *,void *,void *,float *,float *,int,unsigned,int);
 tceDamagePose_t pose; tceDamageVolume_t source[3];vec3_t dir={1,0,0},point={-100,0,32};
 int flags,w,angle,lean,i,j,n=0,y,z,mask,amounts[3],take;float result,expected;float pitches[]={-90,0,45,90,180,270,360};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(unsigned char *)dll;entity=base+0x226e2a0;part=entity+70*0x57c;damage=(void *)(base+0x589c0);
 patch(base+0x94e30,spawn);patch(base+0x94f30,nop);patch(base+0x8afa0,nop);patch(base+0x8ae80,trace);
 patch(base+0x8daa0,sameTeam);patch(base+0x7d4b0,nop);patch(base+0x21780,nop);patch(base+0x65470,nop);
 memset(&pose,0,sizeof(pose));fractions[0]=fractions[1]=fractions[2]=1;
 for(flags=0;flags<16;++flags)for(w=0;w<64;++w)for(angle=0;angle<7;++angle)for(lean=-1;lean<=1;++lean){
  pose.prone=flags&1;pose.ducked=flags&2;pose.aiming=flags&4;pose.enlarged=flags&8;pose.weapon=w;pose.lean=lean*16;
  pose.angles[0]=pitches[angle];pose.angles[1]=angle*42.75f;pose.angles[2]=0;pose.origin[0]=10;pose.origin[1]=-42;pose.origin[2]=76;
  setup(&pose);result=damage(entity,entity,entity,dir,point,15,0x20,7);TCE_DamageVolumes(&pose,source);
  if(calls!=3||result!=1)return 3;
  for(i=0;i<3;++i)for(j=0;j<9;++j)if(fabs(((float *)&source[i])[j]-((float *)&captured[i])[j])>0.0002f){
   printf("Volume mismatch flags%d weapon%d pitch%g lean%d volume%d field%d source%.9g original%.9g\n",flags,w,pose.angles[0],lean,i,j,((float *)&source[i])[j],((float *)&captured[i])[j]);return 4;}
  ++n;
 }
 memset(&pose,0,sizeof(pose));pose.weapon=39;TCE_DamageVolumes(&pose,source);
 for(mask=0;mask<27;++mask)for(y=-24;y<=24;y+=3)for(z=-20;z<=60;z+=2){
  fractions[0]=(mask%3)*0.5f;fractions[1]=((mask/3)%3)*0.5f;fractions[2]=(mask/9)*0.5f;
  point[1]=(float)y;point[2]=(float)z;setup(&pose);result=damage(entity,entity,entity,dir,point,15,0x20,7);
  take=TCE_DamageWeights(source,fractions,point,dir,1,15,amounts,NULL);
  for(i=0;i<3;++i)if(amounts[i]!=*(int *)(client+0x398+4*i)){printf("Weight mismatch mask%d y%d z%d region%d source%d original%d\n",mask,y,z,i,amounts[i],*(int *)(client+0x398+4*i));return 5;}
  expected=take?1.0f-take/15.0f:1;if(expected<.1f)expected=.1f;
  if(fabs(result-expected)>0.000001f){printf("Return mismatch %g/%g\n",result,expected);return 6;}++n;
 }
 printf("PASS %d original G_Damage body geometry/weight/return comparisons\n",n);return 0;
}
