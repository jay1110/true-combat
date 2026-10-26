#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_main.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
int main(int argc,char**argv){HMODULE h;unsigned char*b;int n,mask,index,mode,sp,j,cases=0;const int modes[]={2,4,5,7};static char names[8][20],campaigns[8][20];assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;
for(j=0;j<8;++j){sprintf(names[j],"obj_map_%d",j);sprintf(campaigns[j],"campaign_%d",j);uiInfo.mapList[j].mapName=names[j];*(char**)(b+0x2b52ac+j*0x88)=names[j];uiInfo.campaignList[j].campaignName=campaigns[j];*(char**)(b+0x2b96b4+j*0x74)=campaigns[j];}
ui_gameType.integer=*(int*)(b+0x2d926c)=1;
for(n=0;n<=8;++n)for(mask=0;mask<256;++mask){uiInfo.mapCount=*(int*)(b+0x2b52a8)=n;/* deliberately unrelated count: original campaign branch uses mapCount */uiInfo.campaignCount=0;
for(j=0;j<8;++j){uiInfo.mapList[j].active=*(int*)(b+0x2b530c+j*0x88)=(mask&(1<<j))?7:0;uiInfo.campaignList[j].typeBits=*(int*)(b+0x2b9720+j*0x74)=(mask&(1<<j))?12:8;}
for(index=-1;index<=9;++index){int a=((int(__cdecl*)(int))(b+0xc610))(index),s=UI_GetIndexFromSelection(index);assert(a==s);++cases;
for(mode=0;mode<4;++mode)for(sp=0;sp<2;++sp){int oa=99,sa=88;const char*o,*r;ui_netGameType.integer=*(int*)(b+0x2dc50c)=modes[mode];uiInfo.gameTypes[1].gtEnum=*(int*)(b+0x2b328c+16)=modes[(mode+1)%4];o=((const char*(__cdecl*)(int,int,int*))(b+0xcff0))(sp,index,&oa);r=UI_SelectedMap(sp,index,&sa);if(oa!=sa||strcmp(o,r)){fprintf(stderr,"selection mismatch n%d mask%d index%d mode%d sp%d\n",n,mask,index,mode,sp);return 1;}if(*o)assert(o==r);++cases;}}}
printf("%d original map-selection comparisons passed\n",cases);FreeLibrary(h);return 0;}
