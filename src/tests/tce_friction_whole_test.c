#include "../game/bg_pmove.c"
#include <windows.h>
#include <stdio.h>
int main(int argc,char **argv) {
 HMODULE dll;byte *base,raw[0x700],rawpm[0x200],ext[0x100];
 playerState_t ps;pmove_t move;pmoveExt_t extension;
 int v,t,i,cases=0;float times[]={0,.008f,.016f,.033f,.1f};int elapsed[]={0,250,251,299,349,350};
 float speeds[]={0,.5f,1,2.99f,3,49,50,127.25f,320};void (__cdecl *original)(void);
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;original=(void*)(base+0xb950);
 /* Preserve original parameters; ordinary game constants, no effect hooks. */
 pm_stopspeed=*(float*)(base+0x9e948);pm_friction=*(float*)(base+0x9e974);
 pm_waterfriction=*(float*)(base+0x9e978);pm_slagfriction=*(float*)(base+0x9e97c);
 pm_spectatorfriction=*(float*)(base+0x9e988);pm_ladderfriction=*(float*)(base+0x9e984);
 for(v=0;v<32768;v++)for(t=0;t<5;t++) {
  memset(raw,0,sizeof(raw));memset(rawpm,0,sizeof(rawpm));memset(ext,0,sizeof(ext));
  memset(&ps,0,sizeof(ps));memset(&move,0,sizeof(move));memset(&extension,0,sizeof(extension));memset(&pml,0,sizeof(pml));
  *(void**)(base+0x4ba9edc)=rawpm;*(void**)rawpm=raw;*(void**)(rawpm+4)=ext;
  pm=&move;move.ps=&ps;move.pmext=&extension;
  ps.pm_type=*(int*)(raw+4)=(v/9)%3;
  ps.velocity[0]=speeds[v%9];ps.velocity[1]=(v&32)?-ps.velocity[0]*.5f:0;ps.velocity[2]=(v&64)?-53.25f:77.5f;
  memcpy(raw+0x20,ps.velocity,12);
  pml.walking=*(int*)(base+0x4ba9e6c)=!!(v&128);
  /* Original local preservedZ is undefined when extension is set off ground. */
  pml.tceGroundExtension=*(int*)(base+0x4ba9ed4)=pml.walking&&!!(v&256);
  pml.ladder=*(int*)(base+0x4ba9ecc)=!!(v&512);
  pml.groundTrace.surfaceFlags=*(int*)(base+0x4ba9ea0)=(v&1024)?SURF_SLICK:0;
  ps.pm_flags=*(int*)(raw+0xc)=(v&2048)?PMF_TIME_KNOCKBACK:0;
  ps.eFlags=*(int*)(raw+0x68)=(v&4096)?0x01000000:0;
  move.waterlevel=*(int*)(rawpm+0x110)=(v/8192)%4;
  move.watertype=*(int*)(rawpm+0x10c)=(v&16)?CONTENTS_SLIME:CONTENTS_WATER;
  move.cmd.serverTime=*(int*)(rawpm+0xc)=1000;
  extension.dodgeTime=*(int*)(ext+0x30)=1000-elapsed[(v/27)%6];
  pml.frametime=*(float*)(base+0x4ba9e64)=times[t];original();PM_Friction();
  for(i=0;i<3;i++){float expected=*(float*)(raw+0x20+i*4);
   if(memcmp(&expected,&ps.velocity[i],4)&&!(expected!=expected&&ps.velocity[i]!=ps.velocity[i])){
    printf("friction mismatch v%d t%d axis%d %.9g/%.9g\n",v,t,i,expected,ps.velocity[i]);return 1;}}
  cases++;
 }
 printf("PASS whole PM_Friction %d cases exact float results\n",cases);return 0;
}
