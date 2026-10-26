#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_main.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
int main(int argc,char**argv){HMODULE h;unsigned char*b;int font,i,j,k,l,cases=0;const char*texts[]={NULL,"","A","ABC","^1A^2Bc","^^A^","\200\377A","A B\nC","^1^2^3"};float scales[]={-.7f,0,.1f,.3f,.99999994f,1,1.7f,3};float glyphscales[]={.1f,.3f,1,1.33333337f};assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;assert(sizeof(fontInfo_t)==0x5044);
for(font=0;font<4;++font){fontInfo_t*f=&uiInfo.uiDC.Assets.fonts[font];memset(f,0,sizeof(*f));for(i=0;i<256;++i)f->glyphs[i].xSkip=(i*37)%151-7;f->glyphScale=glyphscales[font];memcpy(b+0x292160+font*0x5044,f,sizeof(*f));*(int*)(b+0x2d8168)=uiInfo.activeFont=font;
for(i=0;i<9;++i)for(j=0;j<8;++j)for(k=-1;k<=12;++k)for(l=0;l<2;++l){int a,c;if(l){a=((int(__cdecl*)(const char*,float,int))(b+0x4270))(texts[i],scales[j],k);c=Text_Width(texts[i],scales[j],k);}else{a=((int(__cdecl*)(const char*,float,int,fontInfo_t*))(b+0x41f0))(texts[i],scales[j],k,f);c=Text_Width_Ext(texts[i],scales[j],k,f);}if(a!=c){fprintf(stderr,"width mismatch font%d text%d scale%d limit%d wrapper%d original%d source%d\n",font,i,j,k,l,a,c);return 1;}++cases;}}
printf("%d original UI text-width comparisons passed\n",cases);FreeLibrary(h);return 0;}
