#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_weapons.c"
#include <windows.h>
#include <stdio.h>
static snapshot_t snapshot;
static unsigned char rawSnapshot[0x10000];
static void hook(void *at,void *fn){DWORD o,t;unsigned char*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int*)(p+1)=(unsigned char*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void ent(unsigned char *b,int index,int flags,int type,int weapon,int barrel) {
 unsigned char *raw=b+0x488d800+index*0xa94;centity_t*c=&cg_entities[index];int j;
 memset(raw,0,0xa94);memset(c,0,sizeof(*c));
 *(int*)raw=c->currentState.number=index;*(int*)(raw+4)=type; c->currentState.eType=type==44?ET_MG42_BARREL:type==50?ET_AAGUN:ET_PLAYER;
 *(int*)(raw+8)=c->currentState.eFlags=flags;*(int*)(raw+0xe8)=c->currentState.weapon=weapon;
 *(int*)(raw+0xa8)=c->currentState.modelindex2=barrel;*(int*)(raw+0xa30)=c->tagParent=4;
 *(int*)(raw+0x8c)=c->currentState.otherEntityNum=1;
 for(j=0;j<3;++j) {
  *(float*)(raw+0x18+j*4)=c->currentState.pos.trBase[j]=(float)(index*13+j*7);
  *(float*)(raw+0x3c+j*4)=c->currentState.apos.trBase[j]=(float)(index*7+j*17);
  *(float*)(raw+0x710+j*4)=c->lerpOrigin[j]=(float)(index*123+j*7);
  *(float*)(raw+0x71c+j*4)=c->lerpAngles[j]=(float)(index*3+j*11);
  *(float*)(raw+0x9fc+j*4)=c->mountedMG42Flash.origin[j]=(float)(index*217+j*17);
 }
}
int main(int argc,char **argv) {
 unsigned char *b;HMODULE dll;int flags[]={0,EF_MG42_ACTIVE,EF_AAGUN_ACTIVE,EF_MOUNTEDTANK,EF_PRONE};
 int local,f,type,weapon,third,draw,barrel,i,j,n=0,expectedRet,actualRet;
 vec3_t actual,expected;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(unsigned char*)dll;
 cg.snap=&snapshot;*(void**)(b+0x40352c4)=rawSnapshot;
 snapshot.ps.clientNum=*(int*)(rawSnapshot+0xcc)=0;
 snapshot.ps.viewheight=*(int*)(rawSnapshot+0xe8)=42;
 snapshot.ps.viewlocked_entNum=*(int*)(rawSnapshot+0x440)=3;
 snapshot.numEntities=*(int*)(rawSnapshot+0x5d8)=2;
 snapshot.entities[0].number=*(int*)(rawSnapshot+0x5dc)=5;
 snapshot.entities[1].number=*(int*)(rawSnapshot+0x5dc+0x120)=6;
 for(j=0;j<3;++j){snapshot.ps.origin[j]=*(float*)(rawSnapshot+0x40+j*4)=27.f+j*13;
  snapshot.ps.viewangles[j]=*(float*)(rawSnapshot+0xdc+j*4)=3.f+j*47;
  cg.tankflashorg[j]=*(float*)(b+0x40a2438+j*4)=147.f+j*73;}
 for(local=0;local<2;++local)for(f=0;f<5;++f)for(type=0;type<3;++type)for(weapon=0;weapon<64;++weapon)
 for(third=0;third<2;++third)for(draw=0;draw<2;++draw)for(barrel=-5;barrel<6;++barrel) {
  for(i=0;i<7;++i)ent(b,i,flags[f],i>=5?50:type==0?1:type==1?44:50,weapon,barrel);
  cg.renderingThirdPerson=*(int*)(b+0x407deb8)=third;cg_drawGun.integer=*(int*)(b+0x4034d0c)=draw;
  snapshot.ps.eFlags=*(int*)(rawSnapshot+0x94)=flags[f];snapshot.ps.weapon=*(int*)(rawSnapshot+0xd0)=weapon;
  for(j=0;j<3;++j)actual[j]=expected[j]=-913.f+j;
  expectedRet=((int(__cdecl*)(int,float*))(b+0x7b6f0))(local,expected);actualRet=CG_CalcMuzzlePoint(local,actual);
  if(expectedRet!=actualRet){puts("muzzle return");return 1;}
  for(j=0;j<3;++j)if(fabs(actual[j]-expected[j])>.0003f){printf("muzzle local%d flags%x type%d weapon%d third%d draw%d barrel%d axis%d %.9g/%.9g\n",local,flags[f],type,weapon,third,draw,barrel,j,actual[j],expected[j]);return 1;}++n;
 }
 for(i=-100000;i<=100000;++i){vec3_t toward={(float)(i%113)*.13f,-.4f,4.75f};
  expected[0]=actual[0]=i*.03125f;expected[1]=actual[1]=-i*.015625f;expected[2]=actual[2]=(float)(i%17);
  ((void(__cdecl*)(float*,float*))(b+0x75b50))(expected,toward);SnapVectorTowards(actual,toward);
  if(memcmp(expected,actual,sizeof(actual))){printf("snap%d\n",i);return 1;}++n;
 }
 printf("%d whole muzzle and snap original comparisons passed\n",n);return 0;
}
