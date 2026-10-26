#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_servercmds.c"
#include <windows.h>
#include <stdio.h>
static byte*base;static int run,calls,slot,state[3];static bufferedVoiceChat_t payload;
static void play(bufferedVoiceChat_t*p){calls++;slot=run?((byte*)p-(base+0x1dc36c0))/328:p-voiceChatBuffer;payload=*p;state[0]=run?*(int*)(base+0x4081528):cg.voiceChatTime;state[1]=run?*(int*)(base+0x408152c):cg.voiceChatBufferIn;state[2]=run?*(int*)(base+0x4081530):cg.voiceChatBufferOut;}
static void hook(void*at,void*fn){DWORD old,t;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t);}
int main(int argc,char**argv){HMODULE m;void(__cdecl*originalPlay)(void);void(__cdecl*originalAdd)(void*);bufferedVoiceChat_t input,expected;int k,op,j,c,s,st[3];byte*bytes=(byte*)&input;
 if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;base=(byte*)m;originalPlay=(void*)(base+0x5ca90);originalAdd=(void*)(base+0x5cb00);if(sizeof(input)!=328)return 3;hook(base+0x5c6f0,play);hook(CG_PlayVoiceChat,play);
 for(k=0;k<8192;k++)for(op=0;op<2;op++){
  for(j=0;j<sizeof(input);j++)bytes[j]=(byte)(k*13+j*7);for(j=0;j<32;j++){voiceChatBuffer[j]=input;voiceChatBuffer[j].clientNum=j;voiceChatBuffer[j].snd=(k&1)?0:j+1;}
  memcpy(base+0x1dc36c0,voiceChatBuffer,sizeof(voiceChatBuffer));cg.time=*(int*)(base+0x407dea4)=k-4000;cg.voiceChatTime=*(int*)(base+0x4081528)=cg.time+k%3-1;cg.voiceChatBufferIn=*(int*)(base+0x408152c)=(k/32)%32;cg.voiceChatBufferOut=*(int*)(base+0x4081530)=k%32;
  run=1;calls=0;slot=-1;memset(state,0,sizeof(state));memset(&payload,0,sizeof(payload));if(op)originalAdd(&input);else originalPlay();c=calls;s=slot;memcpy(st,state,sizeof(st));expected=payload;
  run=0;calls=0;slot=-1;memset(state,0,sizeof(state));memset(&payload,0,sizeof(payload));if(op)CG_AddBufferedVoiceChat(&input);else CG_PlayBufferedVoiceChats();
  if(c!=calls||s!=slot||memcmp(st,state,sizeof(st))||memcmp(&expected,&payload,sizeof(payload))||memcmp(base+0x1dc36c0,voiceChatBuffer,sizeof(voiceChatBuffer))||cg.voiceChatTime!=*(int*)(base+0x4081528)||cg.voiceChatBufferIn!=*(int*)(base+0x408152c)||cg.voiceChatBufferOut!=*(int*)(base+0x4081530)){printf("voice buffer case%d op%d calls%d/%d slot%d/%d\n",k,op,c,calls,s,slot);return 1;}
 }puts("16384 whole voice buffer comparisons passed (8192 each)");return 0;}
