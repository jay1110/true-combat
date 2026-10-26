#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_players.c"
#include "../cgame/cg_snapshot.c"
#include <windows.h>
#include <stdio.h>
static byte raw[0xa94],packed[0xa94],snapraw[32];static centity_t cent,expected;static snapshot_t snap;static int run,n,events[16][12];static byte*module;
static void transfer(centity_t*c,byte*r,int read){
#define F(off,field) if(read)memcpy(&c->field,r+off,sizeof(c->field));else memcpy(r+off,&c->field,sizeof(c->field))
 F(0,currentState.number);F(4,currentState.eType);F(8,currentState.eFlags);F(12,currentState.pos);F(48,currentState.apos);F(92,currentState.origin);F(116,currentState.angles);F(172,currentState.clientNum);F(192,currentState.eventSequence);F(236,currentState.legsAnim);F(240,currentState.torsoAnim);F(272,currentState.effect2Time);
 F(0x248,muzzleFlashTime);F(0x24c,overheatTime);F(0x250,previousEvent);F(0x254,previousEventSequence);F(0x25c,trailTime);F(0x260,miscTime);F(0x264,soundTime);F(0x268,pe.legs);F(0x2bc,pe.torso);F(0x418,pe.painAnimTorso);F(0x41c,pe.painAnimLegs);F(0x6ec,pe.animSpeed);F(0x6f8,rawOrigin);F(0x704,rawAngles);F(0x710,lerpOrigin);F(0x71c,lerpAngles);F(0x740,headJuncIndex);F(0x744,headJuncIndex2);F(0x758,lastFuseSparkTime);F(0x910,highlightTime);F(0x914,highlighted);F(0x930,moving);F(0xa2c,akimboFire);
#undef F
}
static void clearframe(centity_t*c,clientInfo_t*ci,lerpFrame_t*l,int anim){int*p=events[n++];p[0]=1;p[1]=c==(run?(void*)raw:&cent);p[2]=run?((byte*)ci-module-0x2580a10)/448:ci-cgs.clientinfo;p[3]=run?(byte*)l-raw:((l==&cent.pe.legs)?0x268:0x2bc);p[4]=anim;memset(l,0x71,84);}
static void evaluate(const trajectory_t*t,int time,vec3_t out,qboolean angle,int spline,float scale){int*p=events[n++];p[0]=2;p[1]=time;p[2]=angle;p[3]=spline;memcpy(p+4,&scale,4);p[5]=run?(byte*)t-raw:(t==&cent.currentState.pos?12:48);out[0]=1.37f+angle;out[1]=-2.9f;out[2]=7.31f;}
static void print(const char*fmt,...){va_list ap;int*p=events[n++];double yaw;p[0]=3;va_start(ap,fmt);p[1]=va_arg(ap,int);yaw=va_arg(ap,double);memcpy(p+2,&yaw,8);va_end(ap);p[4]=strcmp(fmt,"%i ResetPlayerEntity yaw=%i\n");}
static void resetplayer(centity_t*c){int*p=events[n++];p[0]=4;p[1]=c==(run?(void*)raw:&cent);if(run)memset(raw+0x6f8,0x39,24);else{memset(cent.rawOrigin,0x39,12);memset(cent.rawAngles,0x39,12);}}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE m;void(__cdecl*f[2])(void*);int op,k,i,count,copy[16][12];
 if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;module=(byte*)m;if(sizeof(lerpFrame_t)!=84)return 3;f[0]=(void*)(module+0x561e0);f[1]=(void*)(module+0x5fe90);
#define H(off,fn,to) hook(module+off,to);hook(fn,to)
 H(0x53e70,CG_ClearLerpFrameRate,clearframe);H(0x56380,CG_ClearLerpFrame,clearframe);H(0x43f0,TCE_BG_EvaluateTrajectory,evaluate);H(0x47b10,CG_Printf,print);
 for(op=0;op<2;op++){
  if(op){H(0x561e0,CG_ResetPlayerEntity,resetplayer);}
  for(k=0;k<4096;k++){
   memset(&cent,0x5a,sizeof(cent));memset(raw,0x5a,sizeof(raw));cent.currentState.number=k%1024;cent.currentState.clientNum=k%64;cent.currentState.eType=k%4;cent.currentState.eFlags=k%2;cent.currentState.eventSequence=k;cent.currentState.legsAnim=k%27;cent.currentState.torsoAnim=k%31;cent.currentState.effect2Time=k%19-1;cent.rawAngles[0]=k*.13f;cent.rawAngles[1]=k*-.17f;
   transfer(&cent,raw,0);expected=cent;cg.time=*(int*)(module+0x407dea4)=k-700;cg_debugPosition.integer=*(int*)(module+0x4889b4c)=(k/2)%2;cg.snap=&snap;snap.serverTime=*(int*)(snapraw+8)=k+123;*(void**)(module+0x40352c4)=snapraw;
   run=1;n=0;memset(events,0,sizeof(events));f[op](raw);count=n;memcpy(copy,events,sizeof(copy));transfer(&expected,raw,1);
   run=0;n=0;memset(events,0,sizeof(events));if(op)CG_ResetEntity(&cent);else CG_ResetPlayerEntity(&cent);memset(packed,0x5a,sizeof(packed));transfer(&cent,packed,0);
   if(n!=count||memcmp(copy,events,sizeof(copy))||memcmp(raw,packed,sizeof(raw))||memcmp(&cent,&expected,sizeof(cent))){printf("entity reset case%d op%d calls%d/%d\n",k,op,count,n);for(i=0;i<sizeof(raw);i++)if(raw[i]!=packed[i]){printf("raw offset%x expected%02x actual%02x\n",i,raw[i],packed[i]);break;}return 1;}
  }
 }puts("8192 whole entity reset comparisons passed (4096 each)");return 0;}
