#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
static float output[13];static int calls,flags,secondaryNull;
static void fill(float x,float y,float w,float h,float *fg,float *secondary,float *bg,float percent,int f) {
 output[0]=x;output[1]=y;output[2]=w;output[3]=h;output[4]=percent;memcpy(output+5,fg,16);memcpy(output+9,bg,16);flags=f;secondaryNull=secondary==NULL;calls++;
}
static void hook(void *at,void *fn){DWORD old,t;byte *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t);}
int main(int argc,char **argv){HMODULE module;byte *b;void(__cdecl *original)(float,float,float,float,float);float expected[13],x,y,w,h,p;int k;
 if(argc!=2 || !(module=LoadLibraryA(argv[1])))return 2;b=(byte*)module;original=(void*)(b+0x29b00);hook(b+0x298b0,fill);hook(CG_FilledBar,fill);
 for(k=0;k<12000;k++){x=(k%701-350)/7.f;y=(k%991-400)/11.f;w=(k%151-50)/3.f;h=(k%79-20)/9.f;p=(k%401-100)/100.f;
 calls=flags=secondaryNull=0;original(x,y,w,h,p);memcpy(expected,output,sizeof(output));if(calls!=1||flags!=24||!secondaryNull)return 3;
 calls=flags=secondaryNull=0;memset(output,0,sizeof(output));CG_HorizontalPercentBar(x,y,w,h,p);
 if(calls!=1||flags!=24||!secondaryNull||memcmp(expected,output,sizeof(output))){printf("horizontal percent case%d\n",k);return 1;}}
 puts("12000 whole CG_HorizontalPercentBar comparisons passed");return 0;}
