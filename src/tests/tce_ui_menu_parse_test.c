#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_shared.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
static const char **tokens;static int pos,reads,errors,callbacks,inits,posts;static void*target;static char trace[4096];
static int readtoken(int h,pc_token_t*t){assert(h==37);++reads;if(!tokens[pos])return 0;memset(t,0,sizeof(*t));strcpy(t->string,tokens[pos++]);return 1;}
static void error(int h,char*fmt,...){char s[256];va_list a;assert(h==37);va_start(a,fmt);vsprintf(s,fmt,a);va_end(a);strcat(trace,s);++errors;}
static qboolean accept(itemDef_t*m,int h){assert((void*)m==target&&h==37);++callbacks;strcat(trace,"A;");return qtrue;}
static qboolean reject(itemDef_t*m,int h){assert((void*)m==target&&h==37);++callbacks;strcat(trace,"R;");return qfalse;}
static void init(menuDef_t*m){assert((void*)m==target);++inits;strcat(trace,"I;");}
static void post(menuDef_t*m){assert((void*)m==target);++posts;strcat(trace,"P;");}
static void reset(void*t,const char**s){target=t;tokens=s;pos=reads=errors=callbacks=inits=posts=0;trace[0]=0;}
static void patch(void*at,void*fn){unsigned char*p=at;DWORD o,t;assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
int main(int argc,char**argv){HMODULE h;unsigned char*b;keywordHash_t keys[2]={{"accept",accept,NULL},{"reject",reject,NULL}};const char*streams[][9]={{NULL},{"bad",NULL},{"{","}",NULL},{"{",NULL},{"{","accept","}",NULL},{"{","reject","}",NULL},{"{","unknown","accept","}",NULL},{"{","ACCEPT","accept","}",NULL},{"{","unknown","other","reject",NULL},{"{suffix","}suffix",NULL},{"{","accept","accept","accept","}",NULL}};int i,j,cases=0,contract,referenceSlot;unsigned char raw[0x718];menuDef_t source;assert(argc==2||argc==3);contract=argc==3&&!strcmp(argv[2],"--capacity-contract");assert(argc==2||contract);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;
patch(b+0x20dd0,readtoken);patch(trap_PC_ReadToken,readtoken);patch(b+0x1170,error);patch(PC_SourceError,error);patch(b+0x1d830,init);patch(Menu_Init,init);patch(b+0x154e0,post);patch(Menu_PostParse,post);
/* Shared fabricated keyword nodes, real original/source hash lookup. */
memset(menuParseKeywordHash,0,sizeof(menuParseKeywordHash));KeywordHash_Add(menuParseKeywordHash,&keys[0]);KeywordHash_Add(menuParseKeywordHash,&keys[1]);memcpy(b+0x28e9a0,menuParseKeywordHash,sizeof(menuParseKeywordHash));
for(i=0;i<11;++i){int r,p,n,e,k;char expected[4096];memset(raw,0x35,sizeof(raw));memset(&source,0x35,sizeof(source));reset(raw,streams[i]);r=((int(__cdecl*)(int,void*))(b+0x1fa80))(37,raw);p=pos;n=reads;e=errors;k=callbacks;strcpy(expected,trace);reset(&source,streams[i]);assert(Menu_Parse(37,&source)==r);assert(pos==p&&reads==n&&errors==e&&callbacks==k&&!strcmp(trace,expected));++cases;
for(j=0;j<=(contract?MAX_MENUS:128);++j){int count,ni,np;referenceSlot=contract&&j>=128?(j<MAX_MENUS?0:128):j;reset(b+0x255da0+referenceSlot*0x718,streams[i]);*(int*)(b+0x245fb8)=referenceSlot;((void(__cdecl*)(int))(b+0x1fb60))(37);count=*(int*)(b+0x245fb8);p=pos;n=reads;e=errors;k=callbacks;ni=inits;np=posts;strcpy(expected,trace);reset(&Menus[j],streams[i]);menuCount=j;Menu_New(37);assert(menuCount==count+(j-referenceSlot)&&pos==p&&reads==n&&errors==e&&callbacks==k&&inits==ni&&posts==np&&!strcmp(trace,expected));++cases;}}
if(contract)printf("%d UI menu parser checks passed: exact original128-menu prefix plus normalized256-menu extension contract; not strict capacity parity.\n",cases);else printf("%d original menu parse/new comparisons passed\n",cases);FreeLibrary(h);return 0;}
