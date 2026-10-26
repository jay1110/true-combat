#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
static byte clients[64][0x2000];
static gclient_t nativeClients[64];
static jmp_buf fatalJump;
static int fatalCalls;
static char fatalText[1024];
static void fatalSink(const char *text){++fatalCalls;Q_strncpyz(fatalText,text,sizeof(fatalText));longjmp(fatalJump,1);}
static int hook(void*p,void*f){DWORD old,tmp;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);return 1;}
static int originalIndex(void *p,byte *teams){return p?((byte*)p-teams)/80:-1;}
static int sourceIndex(fireteamData_t *p){return p?(int)(p-level.fireTeams):-1;}
int main(int argc,char **argv){
 HMODULE dll;byte *base,*teams,*entities;int(__cdecl*lookup[2])(int,void**);void*(__cdecl*publicTeam)(int);
 int scene,i,j,mode,client,opt,n=0,publicCount=0,fatalCount=0;
 if(argc!=2||sizeof(fireteamData_t)!=80||!(dll=LoadLibraryA(argv[1])))return 2;
 base=(byte*)dll;teams=base+0x221b728;entities=base+0x226e2a0;
 lookup[0]=(void*)(base+0x5c110);lookup[1]=(void*)(base+0x5c1b0);publicTeam=(void*)(base+0x5c4d0);
 if(!hook(base+0x8ab30,fatalSink)||!hook(trap_Error,fatalSink))return 2;
 for(i=0;i<64;i++){g_entities[i].client=&nativeClients[i];*(void**)(entities+i*0x57c+0x190)=clients[i];}
 for(scene=0;scene<4096;scene++){
  byte saved[960];
  for(i=0;i<64;i++)nativeClients[i].sess.sessionTeam=*(int*)(clients[i]+0xb30)=(i+scene)&3;
  for(i=0;i<12;i++){
   fireteamData_t *t=&level.fireTeams[i];int length=1+(scene+i*7)%64;
   memset(t,0x5a,sizeof(*t));t->ident=scene+i;t->leader=63-i;
   t->inuse=(scene>>(i%12))&1;t->priv=((scene+i)>>2)&1;
   for(j=0;j<64;j++)t->joinOrder[j]=(char)(j<length?((j+i*5+scene)&63):-1);
   /* A hole terminates membership searches even when later entries match. */
   if(scene&2048)t->joinOrder[1+(scene+i)%63]=-1;
  }
  memcpy(teams,level.fireTeams,960);memcpy(saved,teams,960);
  for(mode=0;mode<2;mode++)for(client=0;client<64;client++)for(opt=0;opt<2;opt++){
   void *out=(void*)1;fireteamData_t *nativeOut=(void*)1;int x,y;
   x=lookup[mode](client,opt?&out:NULL);
   y=mode?G_IsFireteamLeader(client,opt?&nativeOut:NULL):G_IsOnFireteam(client,opt?&nativeOut:NULL);
   if(x!=y||(opt&&originalIndex(out,teams)!=sourceIndex(nativeOut))||memcmp(saved,teams,960)||memcmp(saved,level.fireTeams,960)){
    fprintf(stderr,"fireteam lookup scene%d mode%d client%d opt%d result%d/%d failed\n",scene,mode,client,opt,y,x);return 1;
   }n++;
  }
  for(i=0;i<4;i++){
   void *x=publicTeam(i);fireteamData_t *y=G_FindFreePublicFireteam(i);
   if(originalIndex(x,teams)!=sourceIndex(y)||memcmp(saved,teams,960)||memcmp(saved,level.fireTeams,960)){
    fprintf(stderr,"public fireteam scene%d team%d failed\n",scene,i);return 1;
   }publicCount++;
  }
 }
 /* Test the actual G_Error formatting chain, capture only engine fatal exit. */
 for(mode=0;mode<2;mode++)for(i=0;i<4;i++)for(opt=0;opt<2;opt++){
  int invalid[4]={-1,64,999,7},side;const char *expected=mode?"G_IsFireteamLeader: invalid client":"G_IsOnFireteam: invalid client";
  *(void**)(entities+7*0x57c+0x190)=NULL;g_entities[7].client=NULL;
  for(side=0;side<2;side++){
   void *out=(void*)1;fireteamData_t *nativeOut=(void*)1;fatalCalls=0;fatalText[0]=0;
   if(setjmp(fatalJump)==0){
    if(!side)lookup[mode](invalid[i],opt?&out:NULL);
    else if(mode)G_IsFireteamLeader(invalid[i],opt?&nativeOut:NULL);else G_IsOnFireteam(invalid[i],opt?&nativeOut:NULL);
    fprintf(stderr,"missing fireteam fatal mode%d input%d side%d\n",mode,invalid[i],side);return 1;
   }
   if(fatalCalls!=1||strcmp(fatalText,expected)||out!=(void*)1||nativeOut!=(void*)1){fprintf(stderr,"fireteam fatal text/state mismatch mode%d side%d: %s\n",mode,side,fatalText);return 1;}
  }fatalCount++;
 }
 printf("%d whole membership/leader comparisons, %d public lookup comparisons, %d paired fatal cases passed\n",n,publicCount,fatalCount);return 0;
}
