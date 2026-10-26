#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_shared.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
static char trace[2048];
typedef struct{char format[128];double percent;int used,capacity;} report_t;
static report_t rows[4],expectedRows[4];static int rowCount;
static void output(const char*fmt,...){va_list args;report_t*r;assert(rowCount<4);r=&rows[rowCount++];memset(r,0,sizeof(*r));strcpy(r->format,fmt);va_start(args,fmt);if(strstr(fmt,"%.1f")){double d=va_arg(args,double);int n=va_arg(args,int),cap=va_arg(args,int);r->percent=d;r->used=n;r->capacity=cap;sprintf(trace+strlen(trace),"%s|%.17g,%d,%d;",fmt,d,n,cap);}else strcat(trace,fmt);va_end(args);}
static void patch(void*at,void*fn){unsigned char*p=at;DWORD o,t;assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
int main(int argc,char**argv){HMODULE h;unsigned char*b;int i,cases=0,contract;char expected[2048];assert(argc==2||argc==3);contract=argc==3&&!strcmp(argv[2],"--capacity-contract");assert(argc==2||contract);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;patch(b+0x2380,output);patch(Com_Printf,output);for(i=0;i<1000;++i){*(int*)(b+0x245fc8)=strPoolIndex=(i*397)%STRING_POOL_SIZE;*(int*)(b+0x65f74)=allocPoint=(i*1597)%MEM_POOL_SIZE;trace[0]=0;rowCount=0;((void(__cdecl*)(void))(b+0x14a70))();assert(rowCount==4);memcpy(expectedRows,rows,sizeof(rows));strcpy(expected,trace);trace[0]=0;rowCount=0;String_Report();assert(rowCount==4);if(contract){int j;for(j=0;j<3;j++)assert(!memcmp(&rows[j],&expectedRows[j],sizeof(rows[j])));assert(!strcmp(rows[3].format,expectedRows[3].format)&&rows[3].used==expectedRows[3].used&&rows[3].capacity==MEM_POOL_SIZE&&expectedRows[3].capacity==1536*1024);assert(fabs(rows[3].percent-rows[3].used*100.0/(MEM_POOL_SIZE))<1e-11);}else if(strcmp(trace,expected)){fprintf(stderr,"report mismatch case%d\nORIGINAL:%s\nSOURCE:%s\n",i,expected,trace);return 1;}++cases;}for(i=-2;i<=3;++i){*(int*)(b+0x63f70)=outOfMemory=i;assert(((int(__cdecl*)(void))(b+0x14900))()==UI_OutOfMemory());++cases;}if(contract)printf("%d UI report checks passed: exact headers/string pool and separately validated4194304-byte memory-capacity/usage percentage; not strict capacity parity.\n",cases);else printf("%d original UI memory-report comparisons passed\n",cases);FreeLibrary(h);return 0;}
