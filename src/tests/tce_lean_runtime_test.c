/* Production PM_UpdateLean versus unhooked Windows30009fe0. */
#include "../game/bg_pmove.c"
#include <windows.h>
#include <stdio.h>
static float fraction, vectors[12];static int calls,client,mask;
static void traceLean(trace_t *tr,const vec3_t a,const vec3_t b,const vec3_t c,const vec3_t d,int cl,int ma) {
 calls++;client=cl;mask=ma;memcpy(vectors,a,12);memcpy(vectors+3,b,12);memcpy(vectors+6,c,12);memcpy(vectors+9,d,12);
 memset(tr,0,sizeof(*tr));tr->fraction=fraction;
}
int main(int argc,char **argv) {
 HMODULE dll;unsigned char *base,raw[0x700],rp[0x200];
 playerState_t ps;usercmd_t cmd;pmove_t move;float ev[12],expected;
 int w,g,t,b,f,l,context,k,n=0,ec,em,ecl;int times[]={1,16,100,200};
 void (__cdecl *original)(void *,void *,void *);
 if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;base=(unsigned char *)dll;original=(void *)(base+0x9fe0);
 gearDef.parsed=1;
 for(w=0;w<64;++w)for(g=0;g<16;++g)for(t=0;t<4;++t)for(b=0;b<4;++b)
 for(f=0;f<3;++f)for(l=0;l<3;++l)for(context=0;context<2;++context) {
  memset(&ps,0,sizeof(ps));memset(&cmd,0,sizeof(cmd));memset(&move,0,sizeof(move));memset(&pml,0,sizeof(pml));
  memset(raw,0,sizeof(raw));memset(rp,0,sizeof(rp));
  ps.weapon=w;ps.weaponstate=7;ps.stats[STAT_HEALTH]=(g&1)?0:100;
  ps.stats[STAT_TCE_WEAPON_FLAGS]=(g&2)?0x4000:0;ps.eFlags=(g&4)?EF_PRONE:((g&8)?EF_FIRING:0);
  if(g==15)ps.eFlags=0x408020;
  if(g==14) { ps.stats[STAT_HEALTH]=100;ps.stats[STAT_TCE_WEAPON_FLAGS]=0;ps.eFlags=0; }
  pml.msec=times[t];pml.ladder=g==14;ps.leanf=(float)(l*25-25);
  ps.clientNum=13;ps.viewheight=40;VectorSet(ps.origin,17,27,37);VectorSet(ps.viewangles,t*13,b*24,3);
  cmd.wbuttons=b<<4;cmd.forwardmove=127;cmd.upmove=g==13?1:0;
  move.trace=traceLean;move.ps=&ps;pm=context?&move:NULL;
  memcpy(raw,&ps,0x420);*(void **)(rp+0x124)=traceLean;
  *(void **)(base+0x4ba9edc)=context?rp:NULL;*(int *)(base+0x4ba9e68)=pml.msec;*(int *)(base+0x4ba9ecc)=pml.ladder;
  fraction=f*.5f;calls=0;original(raw,&cmd,rp);expected=*(float *)(raw+0x3c);ec=calls;em=mask;ecl=client;memcpy(ev,vectors,sizeof(ev));
  calls=0;PM_UpdateLean(&ps,&cmd,&move);
  if(fabs(ps.leanf-expected)>.00005f || calls!=ec || (calls && (mask!=em || client!=ecl))) {
   printf("lean mismatch w%d gate%d dt%d button%d fraction%d initial%d context%d %g/%g traces%d/%d\n",w,g,t,b,f,l,context,ps.leanf,expected,calls,ec);return 1;
  }
  for(k=0;calls && k<12;++k)if(fabs(vectors[k]-ev[k])>.0001f){printf("trace vector%d %g/%g\n",k,vectors[k],ev[k]);return 3;}
  *(float *)(raw+0x3c)=ps.leanf;if(memcmp(raw,&ps,0x420)){puts("other state changed");return 4;}
  ++n;
 }
 printf("%d production lean comparisons passed, including collision arguments and pm-null fallback\n",n);return 0;
}
