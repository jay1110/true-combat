#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_main.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
int main(int argc,char **argv){HMODULE h;unsigned char*b,*p;int n,mask,i,q,actual,expected,cases=0;const char*names[]={"one","two","three","four"},*s,*r;
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;
 for(n=0;n<=4;++n)for(mask=0;mask<256;++mask){uiInfo.campaignCount=*(int*)(b+0x2b96ac)=n;
  for(i=0;i<n;++i){p=b+0x2b96b0+i*0x74;memset(p,0,0x74);memset(&uiInfo.campaignList[i],0,sizeof(uiInfo.campaignList[i]));
   uiInfo.campaignList[i].campaignName=*(const char**)(p+4)=names[i];uiInfo.campaignList[i].unlocked=*(int*)(p+0x60)=(mask>>i)&1;
   uiInfo.campaignList[i].order=*(int*)(p+0x6c)=i%2;uiInfo.campaignList[i].typeBits=*(int*)(p+0x70)=(mask>>(i+4))&1;}
  for(q=-1;q<5;++q){actual=expected=-99;r=((const char*(__cdecl*)(int,int*))(b+0xd0c0))(q,&expected);s=UI_SelectedCampaign(q,&actual);assert(actual==expected&&!strcmp(s,r));++cases;}
  for(q=0;q<2;++q){assert(UI_CampaignCount(q)==((int(__cdecl*)(int))(b+0xd740))(q));++cases;}
 }
 printf("%d complete original campaign selection/count comparisons passed\n",cases);FreeLibrary(h);return 0;}
