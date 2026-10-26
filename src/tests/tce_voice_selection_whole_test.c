#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_servercmds.c"
#include <windows.h>
#include <stdio.h>
static voiceChatList_t input,reference;static int randomValue,calls;
static int fixedRandom(void){calls++;return randomValue;}
static void hook(void*at,void*fn){DWORD old,t;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t);}
int main(int argc,char**argv){HMODULE m;byte*b;int(__cdecl*get)(void*,const char*,int*,int*,char**);void*(__cdecl*list)(int);int k,i,j,r,s,a,c,aa,cc,n;char*x,*y;const char*ids[]={"alpha","BETA","gamma","absent",""};
 if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;b=(byte*)m;get=(void*)(b+0x5c600);list=(void*)(b+0x5c6c0);if(sizeof(voiceChat_t)!=0x944||offsetof(voiceChatList_t,voiceChats)!=0x48)return 3;hook(b+0x885b6,fixedRandom);hook(rand,fixedRandom);
 memset(&input,0,sizeof(input));for(i=0;i<4;i++){Q_strncpyz(input.voiceChats[i].id,ids[i],64);for(j=0;j<MAX_VOICESOUNDS;j++){input.voiceChats[i].sounds[j]=i*100+j;input.voiceChats[i].sprite[j]=i*1000+j;Com_sprintf(input.voiceChats[i].chats[j],MAX_CHATSIZE,"row%d variant%d",i,j);}}
 for(k=0;k<65536;k++){
  input.numVoiceChats=k%6-1;for(i=0;i<4;i++)input.voiceChats[i].numSounds=(k/6+i)%31+1;reference=input;randomValue=k&32767;
  a=aa=-731;c=cc=-732;x=y=(char*)1;calls=0;r=get(&reference,ids[(k/186)%5],&a,&c,&x);n=calls;calls=0;s=CG_GetVoiceChat(&input,ids[(k/186)%5],&aa,&cc,&y);
  if(r!=s||a!=aa||c!=cc||n!=calls||(r?(x-(char*)&reference)!=(y-(char*)&input):x!=y)||memcmp(&input,&reference,sizeof(input))){printf("voice selection case%d result%d/%d sound%d/%d rng%d/%d\n",k,r,s,a,aa,n,calls);return 1;}
 }
 for(k=0;k<64*7;k++){int client=k%64,team=k/64-1;void*p;voiceChatList_t*q;cgs.clientinfo[client].team=*(int*)(b+0x2580a98+client*0x1c0)=team;p=list(client);q=CG_VoiceChatListForClient(client);if((byte*)p-b!=(q==&voiceChatLists[0]?0x18d7280:0x1974b08)||(q!=&voiceChatLists[0]&&q!=&voiceChatLists[1])){printf("voice list case%d\n",k);return 1;}}
 puts("65984 whole voice selection comparisons passed (65536 variants,448 teams)");return 0;}
