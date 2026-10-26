#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../game/tce_score.h"
static int cases,count,calls[1000];static char args[1000][32];
static void require(int ok,const char *why){if(!ok){fprintf(stderr,"case %d: %s\n",cases,why);exit(1);}}
static const char *arg(int i){require(i>=0&&i<1000&&count<1000,"argument bounds");calls[count++]=i;return args[i];}
typedef struct {unsigned char *p,bytes[5];} hook_t;
static void hook(hook_t *h,unsigned char *base,int offset,void *fn){DWORD old,tmp;h->p=base+offset;memcpy(h->bytes,h->p,5);require(VirtualProtect(h->p,5,PAGE_EXECUTE_READWRITE,&old),"protect");h->p[0]=0xe9;*(int *)(h->p+1)=(unsigned char *)fn-h->p-5;FlushInstructionCache(GetCurrentProcess(),h->p,5);VirtualProtect(h->p,5,old,&tmp);}
static void unhook(hook_t *h){DWORD old,tmp;require(VirtualProtect(h->p,5,PAGE_EXECUTE_READWRITE,&old),"restore");memcpy(h->p,h->bytes,5);FlushInstructionCache(GetCurrentProcess(),h->p,5);VirtualProtect(h->p,5,old,&tmp);}
int main(int argc,char **argv){
 HMODULE dll;unsigned char *base;hook_t h;tce_score_t scores[64],expectedScores[64];tce_scoreClient_t clients[64];
 unsigned char expectedClients[64*448],projected[64*448];int expectedCalls[1000],n,num,teamScores[2],part,initial,amount,variant,i,j,offset,id;
 const int amounts[]={-1,0,1,2,8,16,32};const int clientIds[]={-5,0,1,32,63,64,999};
 tce_scoreParseContext_t ctx;void (__cdecl *original)(int);
 require(sizeof(tce_score_t)==60,"score layout");require(argc==2,"DLL path");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"load");base=(unsigned char *)dll;original=(void *)(base+0x5f220);hook(&h,base,0x47cb0,arg);
 ctx.numScores=&num;ctx.teamScores=teamScores;ctx.scores=scores;ctx.clients=clients;ctx.argv=arg;
 for(part=0;part<3;++part)for(initial=0;initial<=32;initial+=8)for(amount=0;amount<7;++amount)for(variant=0;variant<14;++variant){
  num=initial;teamScores[0]=17;teamScores[1]=-9;memset(scores,0x35,sizeof(scores));memcpy(base+0x407f5f8,scores,sizeof(scores));
  *(int *)(base+0x407f5d8)=num;memcpy(base+0x407f5e0,teamScores,8);memset(projected,0x53,sizeof(projected));
  for(i=0;i<64;++i){clients[i].team=i%4;clients[i].score=clients[i].powerups=clients[i].classRating=clients[i].previousClassRating=clients[i].playerClass=0x53535353;*(int *)(projected+i*448)=clients[i].team;}
  memcpy(base+0x2580a98,projected,sizeof(projected));memset(args,0,sizeof(args));offset=part==1?4:2;
  strcpy(args[1]," -31");strcpy(args[2],"42 ");sprintf(args[offset-1],"%d",amounts[amount]);
  for(i=0;i<amounts[amount];++i)for(j=0;j<14;++j){id=offset+i*14+j;sprintf(args[id],variant&1?" %d trailing":"%d",j==0?clientIds[(i+variant)%7]:(i+1)*37-j*11-variant);}
  count=0;original(part);n=count;memcpy(expectedCalls,calls,n*sizeof(int));memcpy(expectedScores,base+0x407f5f8,sizeof(scores));memcpy(expectedClients,base+0x2580a98,sizeof(expectedClients));
  count=0;TCE_CG_ParseScore(part,&ctx);require(count==n&&!memcmp(calls,expectedCalls,n*sizeof(int)),"argument sequence");require(num==*(int *)(base+0x407f5d8)&&!memcmp(teamScores,base+0x407f5e0,8),"counters/team scores");require(!memcmp(scores,expectedScores,sizeof(scores)),"entire score array");
  for(i=0;i<64;++i){*(int *)(projected+i*448+8)=clients[i].score;*(int *)(projected+i*448+28)=clients[i].powerups;*(int *)(projected+i*448+0x12c)=clients[i].classRating;*(int *)(projected+i*448+0x130)=clients[i].previousClassRating;*(int *)(projected+i*448+0x134)=clients[i].playerClass;}
  require(!memcmp(projected,expectedClients,sizeof(projected)),"all client state/untouched bytes");++cases;
 }
 unhook(&h);FreeLibrary(dll);printf("%d complete CG_ParseScore original comparisons passed.\n",cases);return 0;
}
