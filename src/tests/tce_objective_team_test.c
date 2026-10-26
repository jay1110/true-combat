#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
extern qboolean G_ScriptAction_SetVIPTeam(gentity_t*,char*);
extern qboolean G_ScriptAction_SetHostageTeam(gentity_t*,char*);
static jmp_buf jump;static int fatals;static char fatalText[1024];
static byte raw[0x57c];static gentity_t ent;
static const char *inputs[]={NULL,""," ","\n\t","0","1","2","-1","-9999","99","000","01","-0","+1","1junk","foo","\"1\"","\"\"","0 1","1 0","// comment\n1","/* comment */ 0","/* missing","\"0 1\"","0.9","1.9","-2.5","{",";","\"2\"","  +0001 trailing","// only comment"};
static int hook(void*p,void*f){DWORD o,t;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);return 1;}
static void fatal(const char*s){++fatals;Q_strncpyz(fatalText,s,sizeof(fatalText));longjmp(jump,1);}
int main(int argc,char**argv){HMODULE dll;byte*b;int(__cdecl*functions[2])(void*,char*);int mode,sc,ix,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;functions[0]=(void*)(b+0x849b0);functions[1]=(void*)(b+0x84a10);if(!hook(b+0x8ab30,fatal)||!hook(trap_Error,fatal))return 2;
 for(mode=0;mode<2;mode++)for(sc=0;sc<128;sc++)for(ix=0;ix<sizeof(inputs)/sizeof(inputs[0]);ix++){
  char a[256],c[256],expected[1024];int x=-999,y=-999,expectedFatals;
  memset(raw,0,sizeof(raw));memset(&ent,0,sizeof(ent));if(inputs[ix]){Q_strncpyz(a,inputs[ix],sizeof(a));Q_strncpyz(c,inputs[ix],sizeof(c));}
  level.tceVipTeam=*(int*)(b+0x2223940)=sc-63;level.tceHostageTeam=*(int*)(b+0x222394c)=sc*31;level.tceHostageActive=*(int*)(b+0x2223950)=sc%5-2;
  fatals=0;fatalText[0]=0;if(!setjmp(jump))x=functions[mode](sc&1?raw:NULL,inputs[ix]?a:NULL);expectedFatals=fatals;Q_strncpyz(expected,fatalText,sizeof(expected));
  fatals=0;fatalText[0]=0;if(!setjmp(jump))y=mode?G_ScriptAction_SetHostageTeam(sc&1?&ent:NULL,inputs[ix]?c:NULL):G_ScriptAction_SetVIPTeam(sc&1?&ent:NULL,inputs[ix]?c:NULL);
  if(x!=y||fatals!=expectedFatals||strcmp(expected,fatalText)||level.tceVipTeam!=*(int*)(b+0x2223940)||level.tceHostageTeam!=*(int*)(b+0x222394c)||level.tceHostageActive!=*(int*)(b+0x2223950)||(inputs[ix]&&(strcmp(a,inputs[ix])||strcmp(c,inputs[ix])))){
   fprintf(stderr,"objective team mode%d state%d input%d result%d/%d fatal%d/%d: %s / %s\n",mode,sc,ix,y,x,fatals,expectedFatals,fatalText,expected);return 1;
  }n++;
 }
 printf("%d whole objective team-script comparisons passed (4096 each, fatal cases included)\n",n);return 0;
}
