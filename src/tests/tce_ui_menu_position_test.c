#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_shared.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
static unsigned char menu[0x718],items[128][0x280],tips[128][0x280];static menuDef_t sm;static itemDef_t si[128],st[128];
int main(int argc,char**argv){HMODULE h;unsigned char*b;int mode,n,full,k,i,cases=0;assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;
for(mode=0;mode<3;++mode){((void(__cdecl*)(void*))(b+(mode==0?0x15450:mode==1?0x15480:0x154e0)))(NULL);if(mode==0)Item_UpdatePosition(NULL);else if(mode==1)Menu_UpdatePosition(NULL);else Menu_PostParse(NULL);++cases;}
for(mode=0;mode<3;++mode)for(n=0;n<=128;++n)for(full=0;full<2;++full)for(k=0;k<4;++k){float x=(float)(n*.71-91.13),y=(float)(n*.29+3.17);memset(menu,0,sizeof(menu));memset(&sm,0,sizeof(sm));memset(items,0,sizeof(items));memset(tips,0,sizeof(tips));memset(si,0,sizeof(si));memset(st,0,sizeof(st));*(float*)menu=sm.window.rect.x=x;*(float*)(menu+4)=sm.window.rect.y=y;*(int*)(menu+0xbc)=sm.fullScreen=full;*(int*)(menu+0xc0)=sm.itemCount=n;
for(i=0;i<128;++i){float v=(float)(i*.13+1.71);*(void**)(menu+0x514+4*i)=items[i];sm.items[i]=si+i;*(void**)(items[i]+0xec)=(k==3)?NULL:menu;si[i].parent=(k==3)?NULL:&sm;*(float*)(items[i]+16)=si[i].window.rectClient.x=v;*(float*)(items[i]+20)=si[i].window.rectClient.y=-v;*(float*)(items[i]+24)=si[i].window.rectClient.w=17.3f;*(float*)(items[i]+28)=si[i].window.rectClient.h=9.7f;*(float*)(items[i]+0xc0)=si[i].textRect.w=13;*(float*)(items[i]+0xc4)=si[i].textRect.h=19;
if(k==1||k==2){*(void**)(items[i]+0x270)=tips[i];si[i].toolTipData=st+i;*(float*)(tips[i]+16)=st[i].window.rectClient.x=k==1?10:700;*(float*)(tips[i]+20)=st[i].window.rectClient.y=3;*(float*)(tips[i]+24)=st[i].window.rectClient.w=20;}}
if(mode==0){((void(__cdecl*)(void*))(b+0x15450))(items[0]);Item_UpdatePosition(si);}else if(mode==1){((void(__cdecl*)(void*))(b+0x15480))(menu);Menu_UpdatePosition(&sm);}else{((void(__cdecl*)(void*))(b+0x154e0))(menu);Menu_PostParse(&sm);}assert(!memcmp(menu,&sm.window.rect,16));for(i=0;i<128;++i){if(memcmp(items[i],&si[i].window.rect,16)||memcmp(tips[i],&st[i].window.rect,16)||*(float*)(tips[i]+16)!=st[i].window.rectClient.x){fprintf(stderr,"position mismatch mode%d n%d full%d k%d item%d\n",mode,n,full,k,i);return 1;}assert(*(float*)(items[i]+0xc0)==si[i].textRect.w&&*(float*)(items[i]+0xc4)==si[i].textRect.h);}++cases;}
printf("%d original menu/item position comparisons passed\n",cases);FreeLibrary(h);return 0;}
