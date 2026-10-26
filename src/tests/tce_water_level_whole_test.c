#include "../game/bg_pmove.c"
#include <windows.h>
#include <stdio.h>
static int n,contents[3],args[4];static float points[3][3];static int clients[3];
static int point(const vec3_t p,int client){memcpy(points[n],p,12);clients[n]=client;return contents[n++];}
static void condition(int client,int index,int value,int convert){args[0]=client;args[1]=index;args[2]=value;args[3]=convert;}
static void hook(byte*p,void*fn){DWORD old,t;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t);}
int main(int argc,char**argv){
 HMODULE h;byte*b,raw[0x700],rawpm[0x200];playerState_t ps;pmove_t move;
 int k,i,expectedN,expectedArgs[4],expectedClients[3];float expectedPoints[3][3];
 int masks[]={0,CONTENTS_WATER,CONTENTS_SLIME,CONTENTS_LAVA,0x400,0x800,0x1000,0x1c38};
 void(__cdecl*original)(void);
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(byte*)h;original=(void*)(b+0xef60);
 hook(b+0x25f0,condition);hook((byte*)BG_UpdateConditionValue,condition);
 for(k=0;k<32768;k++){
  memset(&ps,0,sizeof(ps));memset(&move,0,sizeof(move));memset(&pml,0,sizeof(pml));memset(raw,0,sizeof(raw));memset(rawpm,0,sizeof(rawpm));
  pm=&move;move.ps=&ps;move.pointcontents=point;*(void**)(b+0x4ba9edc)=rawpm;*(void**)rawpm=raw;*(void**)(rawpm+0x128)=point;
  ps.clientNum=*(int*)(raw+0xa0)=k%64;ps.viewheight=*(int*)(raw+0xbc)=k%87-30;
  ps.mins[2]=*(float*)(raw+0x3e4)=-30.f+(k%17)*.13f;
  ps.origin[0]=k*.17f;ps.origin[1]=-k*.31f;ps.origin[2]=k*.113f-1024.f;memcpy(raw+0x14,ps.origin,12);
  move.waterlevel=*(int*)(rawpm+0x110)=17;move.watertype=*(int*)(rawpm+0x10c)=19;
  pml.tceContentRestriction=*(int*)(b+0x4ba9ed0)=k%5;
  for(i=0;i<3;i++)contents[i]=masks[(k>>(i*3))&7];
  n=0;memset(points,0,sizeof(points));memset(clients,0,sizeof(clients));original();expectedN=n;memcpy(expectedPoints,points,sizeof(points));memcpy(expectedClients,clients,sizeof(clients));memcpy(expectedArgs,args,sizeof(args));
  n=0;memset(points,0,sizeof(points));memset(clients,0,sizeof(clients));PM_SetWaterLevel();
  if(n!=expectedN||memcmp(expectedPoints,points,sizeof(points))||memcmp(expectedClients,clients,sizeof(clients))||memcmp(expectedArgs,args,sizeof(args))||move.waterlevel!=*(int*)(rawpm+0x110)||move.watertype!=*(int*)(rawpm+0x10c)||pml.tceContentRestriction!=*(int*)(b+0x4ba9ed0)){
   printf("water mismatch%d samples%d/%d level%d/%d restriction%d/%d cond%d/%d\n",k,expectedN,n,*(int*)(rawpm+0x110),move.waterlevel,*(int*)(b+0x4ba9ed0),pml.tceContentRestriction,expectedArgs[1],args[1]);for(i=0;i<n;i++)printf("sample%d z%.9g/%.9g\n",i,expectedPoints[i][2],points[i][2]);return 1;}
 }
 puts("PASS whole PM_SetWaterLevel 32768 original comparisons");return 0;
}
