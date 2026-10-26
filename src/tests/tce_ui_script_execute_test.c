#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_shared.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
static char trace[8192];
static void exec(int when,const char*s){sprintf(trace+strlen(trace),"E%d:%s;",when,s);}
static void unknown(char**p){const char*t=NULL;String_Parse(p,&t);sprintf(trace+strlen(trace),"U:%s;",t?t:"NULL");}
static void abortHandler(itemDef_t*i,qboolean*a,char**p){assert(i);strcat(trace,"abort;");*a=qtrue;}
int main(int argc,char**argv){HMODULE h;unsigned char*b;static displayContextDef_t dc;static byte odc[0x20100];static itemDef_t item;void*saved;DWORD oldProtection,ignored;int mode,k,af,valid,cases=0;char expected[8192],obuf[1024],sbuf[1024];
const char*args[]={""," ","map","\"devmap obj_northport\" trailing","\"\"","//c\nvid_restart","; next","\"set g_gametype 5\" ;","{ value"};
const char*scripts[]={NULL,"","; ; ;","exec \"set g_gametype 2\" ; execNOW \"map obj_delta\" ;","EXEC \"set g_gametype 5\" ; EXECnow \"map obj_northport\"","unknown argument ; exec restart","fadein ; exec unreachable","exec","execNOW \"\" ;","//c\nexec \"set g_gametype 7\" ; fadein ;",";prefix consumed ; exec \"vid_restart\""};
assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;dc.executeText=exec;dc.runScript=unknown;Init_Display(&dc);*(void**)(odc+0xcc)=exec;*(void**)(odc+0x68)=unknown;*(void**)(b+0x245fa4)=odc;String_Init();((void(__cdecl*)(void))(b+0x14af0))();assert(VirtualProtect(b+0x378c4,4,PAGE_READWRITE,&oldProtection));saved=*(void**)(b+0x378c4);*(void**)(b+0x378c4)=abortHandler;assert(!strcmp(commandList[0].name,"fadein"));commandList[0].handler=abortHandler;
for(mode=0;mode<2;++mode)for(k=0;k<9;++k)for(af=0;af<3;++af){char*op=obuf,*sp=sbuf;qboolean oa=af,sa=af;strcpy(obuf,args[k]);strcpy(sbuf,args[k]);trace[0]=0;((void(__cdecl*)(itemDef_t*,qboolean*,char**))(b+(mode?0x17260:0x17210)))(NULL,af==2?NULL:&oa,&op);strcpy(expected,trace);trace[0]=0;if(mode)Script_ExecNOW(NULL,af==2?NULL:&sa,&sp);else Script_Exec(NULL,af==2?NULL:&sa,&sp);assert(!strcmp(expected,trace)&&oa==sa&&(op==NULL)==(sp==NULL));if(op)assert(op-obuf==sp-sbuf);++cases;}
for(k=0;k<11;++k)for(af=0;af<3;++af)for(valid=0;valid<2;++valid){qboolean oa=af,sa=af;trace[0]=0;((void(__cdecl*)(itemDef_t*,qboolean*,const char*))(b+0x17810))(valid?&item:NULL,af==2?NULL:&oa,scripts[k]);strcpy(expected,trace);trace[0]=0;Item_RunScript(valid?&item:NULL,af==2?NULL:&sa,scripts[k]);assert(!strcmp(expected,trace)&&oa==sa);++cases;}
*(void**)(b+0x378c4)=saved;assert(VirtualProtect(b+0x378c4,4,oldProtection,&ignored));assert(FreeLibrary(h));printf("%d original script-execution comparisons passed\n",cases);return 0;}
