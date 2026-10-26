#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_sound.c"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
typedef char layout_script[(sizeof(soundScript_t)==100)?1:-1];
typedef char layout_node[(sizeof(soundScriptSound_t)==1100)?1:-1];
static char events[2048];static int seed,rngCalls;static jmp_buf jump;
static int randomValue(void){++rngCalls;return seed;}
static int reg(const char*s,int compressed){char t[100];sprintf(t,"R:%s:%d;",s,compressed);strcat(events,t);return 701;}
static void play(float*o,int e,int ch,int h,int v){char t[100];sprintf(t,"P:%d:%d:%d:%d:%d;",o!=NULL,e,ch,h,v);strcat(events,t);}
static int length(int h){char t[64];sprintf(t,"L:%d;",h);strcat(events,t);return 333;}
static int stream(const char*a,const char*b,int e,int ch,int v){char t[200];sprintf(t,"S:%s:%s:%d:%d:%d;",a,b?b:"NULL",e,ch,v);strcat(events,t);return 444;}
static void fatal(const char*s){strcat(events,s);longjmp(jump,1);}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;int(__cdecl*original)(soundScript_t*,float*,int);static soundScript_t s;static soundScriptSound_t nodes[3],saved[3];int c,i,j,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x60790);hook(b+0x885b6,randomValue);hook(rand,randomValue);hook(b+0x659d0,reg);hook(trap_S_RegisterSound,reg);hook(b+0x64fe0,play);hook(trap_S_StartSoundVControl,play);hook(b+0x65180,length);hook(trap_S_GetSoundLength,length);hook(b+0x65210,stream);hook(trap_S_StartStreamingSound,stream);hook(b+0x64ba0,fatal);hook(trap_Error,fatal);
 for(c=0;c<16000;++c){static int er,sr,ef,sf;int ec;char expected[2048];soundScriptSound_t expectedNodes[3];vec3_t origin={1,2,3};memset(&s,0,sizeof(s));strcpy(s.name,"fixture");s.channel=c%8;s.attenuation=c%511-127;s.streaming=c&1;s.looping=c&2;s.soundList=c%29?nodes:NULL;
  for(i=0;i<3;++i){memset(&nodes[i],0,sizeof(nodes[i]));nodes[i].numsounds=1+c%16;nodes[i].lastPlayed=(c+i*7)%11-5;nodes[i].next=i<2?&nodes[i+1]:NULL;for(j=0;j<16;++j){sprintf(nodes[i].sounds[j].filename,"node%d/sound%d",i,j);nodes[i].sounds[j].sfxHandle=(c+j)%3?j+11:0;}}
  memcpy(saved,nodes,sizeof(nodes));seed=c%32768;tceFlash.deafness=*(float*)(b+0x40a49cc)=(float)(c%151)*.01f;events[0]=0;rngCalls=0;er=sr=-99;ef=sf=0;if(!setjmp(jump))er=original(&s,c&4?origin:NULL,c%66-1);else ef=1;ec=rngCalls;strcpy(expected,events);memcpy(expectedNodes,nodes,sizeof(nodes));memcpy(nodes,saved,sizeof(nodes));events[0]=0;rngCalls=0;if(!setjmp(jump))sr=CG_SoundPickOldestRandomSound(&s,c&4?origin:NULL,c%66-1);else sf=1;
  if(er!=sr||ef!=sf||ec!=rngCalls||strcmp(expected,events)||memcmp(expectedNodes,nodes,sizeof(nodes))){printf("picker case%d: %s / %s\n",c,expected,events);return 1;}++n;
 }
 printf("%d whole sound-picker comparisons passed\n",n);return 0;
}
