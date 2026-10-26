#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_weapons.c"
#include <windows.h>
#include <stdio.h>
static int alt,calls,client,channel,handle,lookupArg,lookupCalls,originNull;
static int lookup(int w){++lookupCalls;lookupArg=w;return alt;}
static void sound(const vec3_t origin,int c,int ch,int h){++calls;originNull=origin==NULL;client=c;channel=ch;handle=h;}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;static byte rawSnap[2048];static snapshot_t snap;int old,neww,k,n=0;void(__cdecl*zoom)(int,int);void(__cdecl*play)(int,int);float vals[]={-1,59,60,75,90,91};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;zoom=(void*)(b+0x743c0);play=(void*)(b+0x74490);
 for(old=0;old<64;++old)for(neww=0;neww<64;++neww)for(k=0;k<12;++k){float expected;int es,et;
  cg.predictedPlayerState.eFlags=*(int*)(b+0x407df28)=(k&1)?0x40000:0;cg.zoomval=*(float*)(b+0x407f1d0)=33;cg.zoomedScope=*(int*)(b+0x407f1c4)=7;cg.zoomTime=*(int*)(b+0x407f1c8)=-55;cg.time=*(int*)(b+0x407dea4)=1024;cg_zoomDefaultSniper.value=*(float*)(b+0x488cf08)=vals[k/2];
  zoom(old,neww);expected=*(float*)(b+0x407f1d0);es=*(int*)(b+0x407f1c4);et=*(int*)(b+0x407f1c8);CG_SetSniperZoom(old,neww);if(expected!=cg.zoomval||es!=cg.zoomedScope||et!=cg.zoomTime){printf("zoom%d/%d/%d\n",old,neww,k);return 1;}++n;
 }
 hook(b+0x74560,lookup);hook(getAltWeapon,lookup);hook(b+0x64fb0,sound);hook(trap_S_StartSound,sound);*(void**)(b+0x40352c4)=rawSnap;cg.snap=&snap;
 for(old=0;old<64;++old)for(neww=0;neww<64;++neww)for(k=0;k<8;++k){int ec,ecl,ech,eh,ea,el,eo;alt=k&1?neww:-1;cg.predictedPlayerState.ammoclip[old]=*(int*)(b+0x407e150+4*old)=k&2?3:0;cg_weapons[neww].switchSound=*(int*)(b+0x484b04c+neww*0xfd0)=(k&4)?0:100+neww;cgs.media.selectSound=*(int*)(b+0x25886f8)=77;snap.ps.clientNum=*(int*)(rawSnap+0xcc)=old;
  calls=client=channel=handle=lookupCalls=0;lookupArg=-999;originNull=-1;play(old,neww);ec=calls;ecl=client;ech=channel;eh=handle;ea=lookupArg;el=lookupCalls;eo=originNull;calls=client=channel=handle=lookupCalls=0;lookupArg=-999;originNull=-1;CG_PlaySwitchSound(old,neww);if(calls!=ec||client!=ecl||channel!=ech||handle!=eh||lookupArg!=ea||lookupCalls!=el||originNull!=eo){printf("switch%d/%d/%d\n",old,neww,k);return 1;}++n;
 }
 printf("%d whole sniper-zoom/switch-sound comparisons passed\n",n);return 0;
}

