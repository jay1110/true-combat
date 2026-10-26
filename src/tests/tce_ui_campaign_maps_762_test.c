#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_main.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
static char trace[8192];
static int height(const char*s,float scale,int limit){sprintf(trace+strlen(trace),"H:%s;",s?s:"NULL");return 13;}
static int width(const char*s,float scale,int limit){return (int)strlen(s)*7;}
static void draw(float x,float y,float scale,vec4_t c,const char*s,float a,int l,int style){sprintf(trace+strlen(trace),"T:%.9g,%.9g,%d:%s;",x,y,style,s);}
static void pic(float x,float y,float w,float h,int sh){sprintf(trace+strlen(trace),"P:%.9g,%.9g,%.9g,%.9g:%d;",x,y,w,h,sh);}
static int shader(const char*s){sprintf(trace+strlen(trace),"S:%s;",s);return 97;}
static struct{unsigned char*p,bytes[5];}hooks[12];static int hc;
static void patch(void*a,void*f){unsigned char*p=a;DWORD o,t;assert(hc<12);hooks[hc].p=p;memcpy(hooks[hc++].bytes,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].bytes,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}
static void set(const char*n,const char*v){sprintf(trace+strlen(trace),"C:%s:%s;",n,v);}
int main(int argc,char**argv){HMODULE h;byte*b;mapInfo map;byte rawMap[0x88];char expected[8192];int mode,net,cam,unlocked,progress,present,shot,t,align,i,cases=0;rectDef_t r={3.13f,11.71f,83.3f,201.7f};vec4_t color={1,1,1,1};const char*texts[]={NULL,"","Alpha beta\nGamma delta"};
assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;
patch(b+0x4460,height);patch(Text_Height,height);patch(b+0x4270,width);patch(Text_Width,width);patch(b+0x48f0,draw);patch(Text_Paint,draw);patch(b+0x26f0,pic);patch(UI_DrawHandlePic,pic);patch(b+0x20730,shader);patch(trap_R_RegisterShaderNoMip,shader);patch(b+0x20530,set);patch(trap_Cvar_Set,set);
for(mode=0;mode<2;++mode)for(net=0;net<2;++net)for(cam=-1;cam<=3;++cam)for(unlocked=0;unlocked<2;++unlocked)for(progress=0;progress<3;++progress)for(present=0;present<2;++present)for(shot=-1;shot<=1;++shot)for(t=0;t<3;++t)for(align=0;align<3;++align){
memset(&map,0,sizeof(map));memset(rawMap,0,sizeof(rawMap));map.briefing=texts[t];map.imageName="levelshots/obj_delta";map.levelShot=shot;*(const char**)(rawMap+0x78)=map.briefing;*(const char**)(rawMap+8)=map.imageName;*(int*)(rawMap+0x5c)=shot;
uiInfo.campaignCount=*(int*)(b+0x2b96ac)=2;ui_currentCampaign.integer=*(int*)(b+0x2d902c)=cam;ui_currentNetCampaign.integer=*(int*)(b+0x28fc2c)=cam;
for(i=0;i<3;++i){uiInfo.campaignList[i].unlocked=*(int*)(b+0x2b9710+i*0x74)=unlocked;uiInfo.campaignList[i].progress=*(int*)(b+0x2b9714+i*0x74)=progress;uiInfo.campaignList[i].mapInfos[1]=present?&map:NULL;*(void**)(b+0x2b96c8+i*0x74+4)=present?rawMap:NULL;}
trace[0]=0;if(mode)((void(__cdecl*)(rectDef_t*,float,float*,int,int))(b+0xf700))(&r,.2f,color,net,1);else((void(__cdecl*)(rectDef_t*,float,float*,float,float,int,int,int,int))(b+0xf4a0))(&r,.2f,color,7.31f,81.13f,3,align,net,1);strcpy(expected,trace);trace[0]=0;
if(mode)UI_DrawCampaignMapPreview(&r,.2f,color,net,1);else UI_DrawCampaignMapDescription(&r,.2f,color,7.31f,81.13f,3,align,net,1);
if(strcmp(expected,trace)){printf("campaign map mismatch mode%d net%d cam%d u%d p%d present%d shot%d t%d align%d\nO%s\nS%s\n",mode,net,cam,unlocked,progress,present,shot,t,align,expected,trace);return 1;}assert(map.levelShot==*(int*)(rawMap+0x5c));assert(ui_currentCampaign.integer==*(int*)(b+0x2d902c)&&ui_currentNetCampaign.integer==*(int*)(b+0x28fc2c));++cases;}
restore();assert(FreeLibrary(h));printf("%d original campaign map description/preview comparisons passed\n",cases);return 0;}
