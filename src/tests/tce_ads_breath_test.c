#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_draw.c"
#include <windows.h>
#include <stdio.h>
static void*entry;static int clockTime,seed,calls,randomCalls,entity,sound,channel,nullOrigin;
static int rng(void){randomCalls++;return seed;}
static void play(const vec3_t p,int e,int ch,sfxHandle_t s){calls++;entity=e;channel=ch;sound=s;nullOrigin=p==NULL;}
static __declspec(naked) void originalBlock(void){__asm { push esi } __asm { sub esp,8 } __asm { mov esi,clockTime } __asm { jmp dword ptr[entry] }}
static __declspec(naked) void blockExit(void){__asm { add esp,8 } __asm { pop esi } __asm { ret }}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b,raw[0x600];snapshot_t snap;int c,i,expected[7];
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;entry=b+0x22d1e;hook(b+0x22e5b,blockExit);hook(b+0x885b6,rng);hook(rand,rng);hook(b+0x64fb0,play);hook(trap_S_StartSound,play);memset(raw,0,sizeof(raw));memset(&snap,0,sizeof(snap));cg.snap=&snap;*(void**)(b+0x40352c4)=raw;
 for(i=0;i<4;i++)cgs.media.tceBreath[i]=*(int*)(b+0x25888fc+4*i)=40+i;
 for(c=0;c<44004;c++){clockTime=cg.time=5000;cg.tceAdsBreathTime=*(int*)(b+0x40a4968)=c%3==0?3000:2999;cg.predictedPlayerState.weapon=*(int*)(b+0x407df64)=57;cg.predictedPlayerState.stats[STAT_TCE_WEAPON_FLAGS]=*(int*)(b+0x407dfb0)=c%7?4:0;cg.predictedPlayerState.stats[14]=*(int*)(b+0x407dfc8)=c%11001-500;weaponDef[57].scoped=*(float*)(b+0x4c1e094+57*0x1cc)=c%11?1:0;snap.ps.clientNum=*(int*)(raw+0xcc)=c%64;seed=c%32768;
  calls=randomCalls=0;entity=sound=channel=nullOrigin=-1;originalBlock();expected[0]=calls;expected[1]=randomCalls;expected[2]=entity;expected[3]=sound;expected[4]=channel;expected[5]=nullOrigin;expected[6]=*(int*)(b+0x40a4968);calls=randomCalls=0;entity=sound=channel=nullOrigin=-1;CG_TCEPlayAdsBreath();
  if(calls!=expected[0]||randomCalls!=expected[1]||entity!=expected[2]||sound!=expected[3]||channel!=expected[4]||nullOrigin!=expected[5]||cg.tceAdsBreathTime!=expected[6]){printf("ADS breath block case%d phase%d\n",c,cg.predictedPlayerState.stats[14]);return 1;}
 }printf("44004 original ADS breathing BLOCK comparisons passed; not whole Draw2D\n");return 0;
}


