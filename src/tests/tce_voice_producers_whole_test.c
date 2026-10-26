#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_servercmds.c"
#include <windows.h>
#include <stdio.h>
static int calls,trace[64],found;static bufferedVoiceChat_t payload;static char words[9][1024],shared[1024],locationText[1024],idText[1024];static int args[4];static vec3_t pos;
static voiceChatList_t*list(int c){trace[calls++]=100+c;return (void*)1;}
static int get(voiceChatList_t*l,const char*id,int*s,int*p,char**text){trace[calls++]=200+(l!=(void*)1);Q_strncpyz(idText,id,sizeof(idText));*s=77;*p=88;*text="Voice translated input";return found;}
static const char*translate(const char*t){trace[calls++]=300;return "Voice translated output";}
static void add(bufferedVoiceChat_t*p){trace[calls++]=400;payload=*p;}
static const char*arg(int i){trace[calls++]=500+i;Q_strncpyz(shared,words[i],sizeof(shared));return shared;}
static void location(int mode,qboolean only,int client,int color,const char*id,vec3_t org,const char*loc){trace[calls++]=600;args[0]=mode;args[1]=only;args[2]=client;args[3]=color;Q_strncpyz(idText,id,sizeof(idText));if(mode){VectorCopy(org,pos);Q_strncpyz(locationText,loc,sizeof(locationText));}}
static void hook(void*at,void*fn){DWORD old,t;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t);}
static void reset(void){calls=0;memset(trace,0,sizeof(trace));memset(&payload,0,sizeof(payload));memset(args,0,sizeof(args));memset(pos,0,sizeof(pos));idText[0]=locationText[0]=0;}
int main(int argc,char**argv){HMODULE m;byte*b;void(__cdecl*origLoc)(int,int,int,int,const char*,vec3_t,const char*);void(__cdecl*origVoice)(int);int k,op,j,n,copy[64],a[4];bufferedVoiceChat_t expected;char id[1024],loc[1024];vec3_t origin={1.3f,-2.7f,9.1f},position;const char*commands[]={"FireInTheHole","kill_insult","taunt","death_insult","kill_gauntlet","praise","TAUNT"};
 if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;b=(byte*)m;origLoc=(void*)(b+0x5cb30);origVoice=(void*)(b+0x5cd20);
#define H(off,fn,to) hook(b+off,to);hook(fn,to)
 H(0x5c6c0,CG_VoiceChatListForClient,list);H(0x5c600,CG_GetVoiceChat,get);H(0x2a830,CG_TranslateString,translate);H(0x5cb00,CG_AddBufferedVoiceChat,add);H(0x47cb0,CG_Argv,arg);
 for(op=0;op<2;op++){
  if(op){H(0x5cb30,CG_VoiceChatLocation,location);}
  for(k=0;k<4096;k++){int mode=k%4,client=k%68-2;found=(k/4)%2;cg_teamChatsOnly.integer=*(int*)(b+0x25762ec)=(k/8)%2;tceFlash.deafness=*(float*)(b+0x40a49cc)=(k/16)%3==0?.2f:0;cg_noTaunt.integer=*(int*)(b+0x400fcec)=(k/7)%2;
   for(j=0;j<64;j++){Com_sprintf(cgs.clientinfo[j].name,sizeof(cgs.clientinfo[j].name),"^3VoicePlayer%d",j);strcpy((char*)b+0x2580a18+j*0x1c0,cgs.clientinfo[j].name);}
   for(j=1;j<9;j++)Com_sprintf(words[j],sizeof(words[j]),"%d",k+j);Q_strncpyz(words[4],commands[k%7],sizeof(words[4]));Q_strncpyz(words[8],"LOCATION_A",sizeof(words[8]));
   cgs.currentVoiceClient=*(int*)(b+0x3fb7800)=91;reset();if(op)origVoice(mode);else origLoc(mode,k%3,client,'2',"FireInTheHole",origin,k%3==0?NULL:k%3==1?"":"Bridge");n=calls;memcpy(copy,trace,sizeof(copy));expected=payload;memcpy(a,args,sizeof(a));VectorCopy(pos,position);strcpy(id,idText);strcpy(loc,locationText);
   reset();if(op)CG_VoiceChat(mode);else CG_VoiceChatLocation(mode,k%3,client,'2',"FireInTheHole",origin,k%3==0?NULL:k%3==1?"":"Bridge");
   if(n!=calls||memcmp(copy,trace,sizeof(copy))||memcmp(a,args,sizeof(a))||memcmp(position,pos,sizeof(pos))||strcmp(id,idText)||strcmp(loc,locationText)||cgs.currentVoiceClient!=*(int*)(b+0x3fb7800)||expected.clientNum!=payload.clientNum||expected.snd!=payload.snd||expected.sprite!=payload.sprite||expected.voiceOnly!=payload.voiceOnly||strcmp(expected.cmd,payload.cmd)||strcmp(expected.message,payload.message)||memcmp(expected.origin,payload.origin,12)){printf("voice producer case%d op%d calls%d/%d msg[%s]/[%s]\n",k,op,n,calls,expected.message,payload.message);return 1;}
  }
 }puts("8192 whole voice producer comparisons passed (4096 each)");return 0;}
