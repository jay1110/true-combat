#define WIN32_LEAN_AND_MEAN
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
static int equal(const float *a,const float *b) {
 int i;for(i=0;i<9;++i)if(fabs(a[i]-b[i])>0.0000002f)return 0;return 1;
}
int main(int argc,char **argv) {
 const unsigned offsets[]={0xa0de0,0x7e850,0x1a00};
 const float values[]={-1000000000,-16777216,-1000000,-1080,-720,-360,-270,-180,-90.00001f,-90,-89.99999f,-1,-0.0f,0,1,89.99999f,90,90.00001f,179.99998f,180,180.00002f,269.99997f,270,270.00003f,359.99997f,360,720,1080,1000000,16777216,1000000000};
 HMODULE h;unsigned char *base;int m,x,y,z,mask,i,count=0;vec3_t angles,original[3],source[3];
 void (__cdecl *fn)(const float *,float *,float *,float *);
 if(argc!=4)return 2;
 for(m=0;m<3;++m){if(!(h=LoadLibraryA(argv[m+1])))return 3;base=(unsigned char *)h;fn=(void *)(base+offsets[m]);
 for(x=0;x<sizeof(values)/sizeof(values[0]);++x)for(y=0;y<sizeof(values)/sizeof(values[0]);++y)for(z=0;z<sizeof(values)/sizeof(values[0]);++z)for(mask=0;mask<8;++mask) {
  angles[0]=values[x];angles[1]=values[y];angles[2]=values[z];
  for(i=0;i<9;++i)((float *)original)[i]=((float *)source)[i]=123.0f+i;
  fn(angles,mask&1?original[0]:NULL,mask&2?original[1]:NULL,mask&4?original[2]:NULL);
  AngleVectors(angles,mask&1?source[0]:NULL,mask&2?source[1]:NULL,mask&4?source[2]:NULL);
  if(!equal((float *)original,(float *)source)){printf("Angle mismatch module%d angles%g,%g,%g mask%d\n",m,angles[0],angles[1],angles[2],mask);return 1;}++count;
 }
 FreeLibrary(h);
 }
 printf("PASS %d complete AngleVectors comparisons against all3 Windows modules, masks0..7 and +/-1e9degree inputs\n",count);return 0;
}
