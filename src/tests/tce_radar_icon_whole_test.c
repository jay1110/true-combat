#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_draw.c"
#include <windows.h>
#include <stdio.h>
static float values[4];static int shader,calls;
static void draw(float x,float y,float w,float h,qhandle_t s){values[0]=x;values[1]=y;values[2]=w;values[3]=h;shader=s;calls++;}
static void hook(byte*p,void*f){DWORD a,b;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a);p[0]=0xe9;*(int*)(p+1)=(byte*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);}
int main(int argc,char**argv){HMODULE h;byte*b;int k,m,n=0,ec,es,x,y,w,height;float expected[4];vec3_t origin,target;float z[]={-200,-72.01f,-72,-71.99f,0,71.99f,72,72.01f,200};void(__cdecl*original)(int,int,int,int,float*,float*,int,int,int);
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(byte*)h;original=(void*)(b+0x215a0);hook(b+0x29ea0,draw);hook((byte*)CG_DrawPic,draw);
 for(k=0;k<8192;k++)for(m=0;m<4;m++){
  origin[0]=12.25f;origin[1]=-29.5f;origin[2]=47;
  target[0]=origin[0]+(k%31-15)*79.25f;target[1]=origin[1]+((k/31)%31-15)*23.375f;target[2]=origin[2]-z[k%9];
  if(k%97==0)VectorCopy(origin,target);
  cg.predictedPlayerState.viewangles[YAW]=*(float*)(b+0x407df74)=(k%721-360)*.5f;
  x=762+(k%3);y=-30+(k%7);w=59+(k%3);height=57+(k%5);
  calls=0;memset(values,0,sizeof(values));shader=0;original(x,y,w,height,origin,target,11,(m&1)?12:0,(m&2)?13:0);ec=calls;es=shader;memcpy(expected,values,sizeof(values));
  calls=0;memset(values,0,sizeof(values));shader=0;CG_DrawRadarIcon(x,y,w,height,origin,target,11,(m&1)?12:0,(m&2)?13:0);
  if(ec!=calls||es!=shader||memcmp(expected,values,sizeof(values))){printf("radar k%d m%d calls%d/%d shader%d/%d xywh %.9g %.9g %.9g %.9g / %.9g %.9g %.9g %.9g\n",k,m,ec,calls,es,shader,expected[0],expected[1],expected[2],expected[3],values[0],values[1],values[2],values[3]);return 1;}n++;
 }printf("PASS %d whole radar icon original comparisons, exact draw arguments\n",n);return 0;
}
