#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
typedef char string_layout[(sizeof(cg_string_t)==132)?1:-1];
typedef char window_layout[(sizeof(cg_window_t)==0x288)?1:-1];
static void normalize(cg_window_t*w,byte*pool){int i;for(i=0;i<64;i++){byte*p=(byte*)w->lineText[i];if(p>=pool&&p<pool+80*132)w->lineText[i]=(char*)(0x10000+(p-pool));}}
int main(int argc,char**argv){HMODULE dll;byte*b,*pool;int(__cdecl*add)(cg_window_t*,char*);void(__cdecl*remove)(cg_window_t*);int c,op,i,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;pool=b+0x40834b8;add=(void*)(b+0x7d540);remove=(void*)(b+0x7d630);
 for(c=0;c<16000;c++)for(op=0;op<2;op++){cg_window_t ow,sw;char text[128];int er=0,ar=0,len=c%128;
  memset(&ow,0x55,sizeof(ow));ow.lineCount=op?c%65:c%64;sw=ow;
  memset(cg.aStringPool,0x61+(c%20),sizeof(cg.aStringPool));for(i=0;i<80;i++)cg.aStringPool[i].fActive=c%5==0?1:(c%5==1?0:((i+c)%3));memcpy(pool,cg.aStringPool,sizeof(cg.aStringPool));
  for(i=0;i<64;i++){int slot=(i*7+c)%80,kind=(i+c)%5;if(kind<3){ow.lineText[i]=(char*)(pool+slot*132+4);sw.lineText[i]=cg.aStringPool[slot].str;}else ow.lineText[i]=sw.lineText[i]=kind==3?NULL:(char*)0x12345678;}
  for(i=0;i<len;i++)text[i]='A'+(i+c)%26;text[len]=0;
  if(op){remove(&ow);CG_removeStrings(&sw);}else{er=add(&ow,text);ar=CG_addString(&sw,text);}
  normalize(&ow,pool);normalize(&sw,(byte*)cg.aStringPool);
  if(er!=ar||memcmp(&ow,&sw,sizeof(ow))||memcmp(pool,cg.aStringPool,sizeof(cg.aStringPool))){printf("window strings case%d op%d result%d/%d lines%d/%d\n",c,op,er,ar,ow.lineCount,sw.lineCount);return 1;}n++;
 }
 printf("%d whole window string add/remove comparisons passed\n",n);return 0;
}
