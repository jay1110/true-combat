#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_sound.c"
#include <windows.h>
#include <stdio.h>
static int calls,entity,nullOrigin,duration;static void*script;
static int pick(void*s,vec3_t origin,int e){++calls;script=s;entity=e;nullOrigin=origin==NULL;return duration;}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;void(__cdecl*add)(void*);void(__cdecl*update)(void);int c,i,op,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;add=(void*)(b+0x608b0);update=(void*)(b+0x60900);hook(b+0x60790,pick);hook(CG_SoundPickOldestRandomSound,pick);
 for(c=0;c<16000;++c)for(op=0;op<2;++op){int ec,ee,en,eq,et;void*es;void*queue[16];void*newScript=(void*)(0x80000+c*16);cg.numbufferedSoundScripts=*(int*)(b+0x40a28ac)=c%17;cg.bufferedSoundScriptEndTime=*(int*)(b+0x40a28a8)=1000;cg.time=*(int*)(b+0x407dea4)=998+c%5;duration=c%1601-200;
  for(i=0;i<16;++i)cg.bufferSoundScripts[i]=*(void**)(b+0x40a2868+4*i)=(void*)(0x10000+c*32+i*4);
  calls=0;entity=nullOrigin=-99;script=NULL;if(op)add(newScript);else update();ec=calls;ee=entity;en=nullOrigin;es=script;eq=*(int*)(b+0x40a28ac);et=*(int*)(b+0x40a28a8);memcpy(queue,b+0x40a2868,64);
  calls=0;entity=nullOrigin=-99;script=NULL;if(op)CG_AddBufferedSoundScript(newScript);else CG_UpdateBufferedSoundScripts();if(ec!=calls||ee!=entity||en!=nullOrigin||es!=script||eq!=cg.numbufferedSoundScripts||et!=cg.bufferedSoundScriptEndTime||memcmp(queue,cg.bufferSoundScripts,64)){printf("soundqueue case%d op%d\n",c,op);return 1;}++n;
 }
 printf("%d whole buffered-sound controller comparisons passed\n",n);return 0;
}
