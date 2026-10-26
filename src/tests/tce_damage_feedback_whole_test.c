#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
extern void CG_DamageFeedback(int,int,int);
static byte snapshotRaw[0x600];static snapshot_t snapshot;static int calls,randomCount,seed,events[8][8];static vec3_t direction;static float distanceValue;
static int rng(void){return (seed+randomCount++*7919)&32767;}
static void angles(const vec3_t a,vec3_t f,vec3_t r,vec3_t u){int*p=events[calls++];p[0]=1;memcpy(p+1,a,12);p[4]=r==NULL;p[5]=u==NULL;VectorCopy(direction,f);}
static float length(const vec3_t v){int*p=events[calls++];p[0]=2;memcpy(p+1,v,12);return distanceValue;}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE m;byte*b;void(__cdecl*original)(int,int,int);int k,j,i,n,r,copy[8][8],expected[8];viewDamage_t expectedDamage[8];
 if(argc==2&&!strcmp(argv[1],"--offsets")){printf("{\"health\":%u,\"damageTime\":%u}\n",(unsigned)offsetof(snapshot_t,ps.stats[STAT_HEALTH]),(unsigned)offsetof(cg_t,damageTime));return 0;}
 if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;b=(byte*)m;original=(void*)(b+0x56930);hook(b+0x7e850,angles);hook(AngleVectors,angles);hook(b+0x7e670,length);hook(VectorLength,length);hook(b+0x885b6,rng);hook(rand,rng);
 for(k=0;k<32768;k++){int yaw=k%257,pitch=(k/3)%257,damage=k%47-10;float mag;
  if(k%7==0)yaw=pitch=255;
  cg.time=*(int*)(b+0x407dea4)=k%513-200;cg.snap=&snapshot;*(void**)(b+0x40352c4)=snapshotRaw;snapshot.serverTime=*(int*)(snapshotRaw+8)=cg.time+100;snapshot.ps.stats[STAT_HEALTH]=*(int*)(snapshotRaw+0xfc)=k%141-10;
  for(i=0;i<8;i++){cg.viewDamage[i].damageTime=cg.time+((i==k%9)?-21:((i==(k/9)%8)?-20:100));cg.viewDamage[i].damageDuration=20;cg.viewDamage[i].damageX=.17f;cg.viewDamage[i].damageY=-.31f;cg.viewDamage[i].damageValue=19;}memcpy(b+0x4080e50,cg.viewDamage,sizeof(cg.viewDamage));
  cg.attackerTime=*(int*)(b+0x4080de4)=-731;cg.damageTime=*(float*)(b+0x4080ef0)=-732;cg.damageIndex=*(int*)(b+0x4080ef4)=-733;cg.v_dmg_time=*(float*)(b+0x4080f6c)=-734;cg.v_dmg_pitch=*(float*)(b+0x4080f70)=-735;cg.v_dmg_roll=*(float*)(b+0x4080f74)=-736;
  for(i=0;i<3;i++){direction[i]=((k+i*17)%61-30)*.037f;for(j=0;j<3;j++)cg.refdef.viewaxis[i][j]=((k+i*13+j*7)%41-20)*.031f;}mag=sqrt(direction[0]*direction[0]+direction[1]*direction[1]+direction[2]*direction[2]);if(mag)for(i=0;i<3;i++)direction[i]/=mag;memcpy(b+0x407f028,cg.refdef.viewaxis,36);distanceValue=k%4==0?.07f:k%4==1?.1f:1.37f;seed=k*13;
  calls=randomCount=0;memset(events,0,sizeof(events));original(yaw,pitch,damage);n=calls;r=randomCount;memcpy(copy,events,sizeof(copy));memcpy(expectedDamage,b+0x4080e50,sizeof(expectedDamage));memcpy(expected,b+0x4080f6c,12);memcpy(expected+3,b+0x4080ef0,8);expected[5]=*(int*)(b+0x4080de4);
  calls=randomCount=0;memset(events,0,sizeof(events));CG_DamageFeedback(yaw,pitch,damage);
  if(n!=calls||r!=randomCount||memcmp(copy,events,sizeof(copy))||memcmp(expectedDamage,cg.viewDamage,sizeof(expectedDamage))||memcmp(expected,&cg.v_dmg_time,12)||memcmp(expected+3,&cg.damageTime,8)||expected[5]!=cg.attackerTime){printf("damage case%d calls%d/%d rng%d/%d\n",k,n,calls,r,randomCount);for(i=0;i<sizeof(expectedDamage)/4;i++)if(((int*)expectedDamage)[i]!=((int*)cg.viewDamage)[i]){printf("damage word%d expected%08x actual%08x\n",i,((int*)expectedDamage)[i],((int*)cg.viewDamage)[i]);break;}return 1;}
 }puts("32768 whole damage feedback comparisons passed");return 0;}
