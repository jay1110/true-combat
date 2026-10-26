#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
static byte raw[0x57c],rawLocations[8][0x57c];static gentity_t ent,locations[8];static int mode,mask,bad,count,seen[8],prints,printedSize;
static int hook(void*p,void*f){DWORD old,tmp;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);return 1;}
static int pvs(const vec3_t a,const vec3_t c){int i;if(memcmp(a,ent.r.currentOrigin,12))bad=1;for(i=0;i<8;++i)if((const void*)c==(mode?(void*)(rawLocations[i]+0x168):(void*)locations[i].r.currentOrigin))break;if(i==8||count==8){bad=1;return 0;}seen[count++]=i;return(mask>>i)&1;}
static void print(const char*fmt,...){va_list ap;++prints;va_start(ap,fmt);printedSize=va_arg(ap,int);va_end(ap);if(strcmp(fmt,"Com_sprintf: overflow of %i bytes buffer\n"))bad=1;}
static int rawIndex(void*p){int i;if(!p)return -1;for(i=0;i<8;++i)if(p==rawLocations[i])return i;return -2;}
static int sourceIndex(gentity_t*p){return p?(int)(p-locations):-1;}
int main(int argc,char**argv){HMODULE dll;byte*b;void*(__cdecl*find)(void*);int(__cdecl*message)(void*,char*,int);int k,j,n=0;const int sizes[]={1,2,5,16,128};static char text[]="Bridge ^1North / long location label";
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;find=(void*)(b+0x8ff00);message=(void*)(b+0x8ffc0);if(!hook(b+0x8af60,pvs)||!hook(trap_InPVS,pvs)||!hook(b+0x618a0,print)||!hook(Com_Printf,print))return 2;
 for(k=0;k<8192;++k){int expected,expectedCount,expectedSeen[8],length=k%9;VectorSet(ent.r.currentOrigin,k%41-20.25f,k%53-26.5f,k%67-33.75f);memcpy(raw+0x168,ent.r.currentOrigin,12);mask=(k/9)%256;level.locationHead=length?locations:NULL;*(void**)(b+0x2223964)=length?rawLocations:NULL;
  for(j=0;j<8;++j){locations[j].nextTrain=j+1<length?&locations[j+1]:NULL;*(void**)(rawLocations[j]+0x21c)=j+1<length?rawLocations[j+1]:NULL;locations[j].message=*(char**)(rawLocations[j]+0x248)=text;
   if(k%4==0)VectorSet(locations[j].r.currentOrigin,ent.r.currentOrigin[0]+(j%2?1:-1)*32,ent.r.currentOrigin[1],ent.r.currentOrigin[2]);
   else if(k%4==1)VectorSet(locations[j].r.currentOrigin,(k%97-j*13)*.125f,(k%89-j*17)*.375f,(k%83-j*11)*.625f);
   else if(k%4==2)VectorSet(locations[j].r.currentOrigin,(j%3-1)*8192.f,(j%2-1)*8192.f,8192.f);
   else VectorSet(locations[j].r.currentOrigin,(k%31-j*7)*1000.25f,(k%23-j*5)*777.125f,(k%19-j*3)*501.75f);
   memcpy(rawLocations[j]+0x168,locations[j].r.currentOrigin,12);
  }
  mode=1;bad=count=0;expected=rawIndex(find(raw));expectedCount=count;memcpy(expectedSeen,seen,sizeof(seen));if(bad)return 3;mode=0;bad=count=0;if(sourceIndex(Team_GetLocation(&ent))!=expected||bad||count!=expectedCount||memcmp(seen,expectedSeen,count*sizeof(int))){fprintf(stderr,"location case%d mismatch calls%d/%d\n",k,count,expectedCount);return 1;}
  for(j=0;j<5;++j){char originalBuffer[132],nativeBuffer[132];int x,y,expectedPrint,expectedSize;memset(originalBuffer,0x5a,sizeof(originalBuffer));memset(nativeBuffer,0x5a,sizeof(nativeBuffer));mode=1;bad=count=prints=printedSize=0;x=message(raw,originalBuffer+2,sizes[j]);expectedCount=count;expectedPrint=prints;expectedSize=printedSize;memcpy(expectedSeen,seen,sizeof(seen));if(bad)return 3;mode=0;bad=count=prints=printedSize=0;y=Team_GetLocationMsg(&ent,nativeBuffer+2,sizes[j]);if(bad||x!=y||prints!=expectedPrint||printedSize!=expectedSize||memcmp(originalBuffer,nativeBuffer,sizeof(nativeBuffer))||count!=expectedCount||memcmp(seen,expectedSeen,count*sizeof(int))){fprintf(stderr,"location message case%d size%d mismatch\n",k,sizes[j]);return 1;}++n;}
 }
 printf("8192 whole Team_GetLocation and %d whole Team_GetLocationMsg comparisons passed\n",n);return 0;
}
