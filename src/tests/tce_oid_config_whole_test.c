#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
extern void CG_ParseOIDInfo(int num);
extern void CG_ParseOIDInfos(void);
static char configs[18][1024];
static int nativeCall, calls, order[18], nullIndex;
static const char *config(int num) {
 int i=num-(nativeCall?0x313:CS_OID_DATA);
 if(calls>=18||i<0||i>=18){puts("invalid config callback");ExitProcess(3);}
 order[calls++]=i;
 return i==nullIndex?NULL:configs[i];
}
static void hook(void *at,void *fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE m;byte*b;void(__cdecl*one)(int);void(__cdecl*all)(void);int k,op,i,j,n,expectedOrder[18];byte expected[18*96];
 const char*keys[]={"s","cia","cix","o","e","n","x","y","z"};
 if(argc==2&&!strcmp(argv[1],"--offset")){printf("%u\n",(unsigned)((byte*)cgs.oidInfo-(byte*)&cgs));return 0;}
 if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;b=(byte*)m;one=(void*)(b+0x5b4c0);all=(void*)(b+0x5b680);
 if(sizeof(oidInfo_t)!=96||MAX_OID_TRIGGERS!=18)return 3;
 hook(b+0x481a0,config);hook(CG_ConfigString,config);
 for(op=0;op<2;op++)for(k=0;k<4096;k++){
  for(i=0;i<32;i++)cgs.gameShaders[i]=*(int*)(b+0x257f14c+4*i)=1000+i*37+k;
  nullIndex=(k%23)-1;
  for(i=0;i<18;i++){configs[i][0]=0;for(j=0;j<9;j++)if((k+i)&(1<<j)){
    char val[180],part[240];
    if(j==5){memset(val,'A'+i,(k%3==0)?100:12);val[(k%3==0)?100:12]=0;}
    else if(j==1||j==2)Com_sprintf(val,sizeof(val),"%d",(k+i+j)%32);
    else Com_sprintf(val,sizeof(val)," %d.9tail",(k-2048)*(j+1)+i);
    Com_sprintf(part,sizeof(part),"\\%s\\%s",keys[j],val);strcat(configs[i],part);
   }}
  memset(cgs.oidInfo,0xa5,sizeof(cgs.oidInfo));memset(b+0x3fdecbc,0xa5,sizeof(expected));
  nativeCall=1;calls=0;memset(order,0,sizeof(order));if(op)all();else one(0x313+k%18);
  memcpy(expected,b+0x3fdecbc,sizeof(expected));n=calls;memcpy(expectedOrder,order,sizeof(order));
  nativeCall=0;calls=0;memset(order,0,sizeof(order));if(op)CG_ParseOIDInfos();else CG_ParseOIDInfo(CS_OID_DATA+k%18);
  if(calls!=n||memcmp(order,expectedOrder,sizeof(order))||memcmp(expected,cgs.oidInfo,sizeof(expected))){for(i=0;i<sizeof(expected);i++)if(expected[i]!=((byte*)cgs.oidInfo)[i])break;printf("OID case%d op%d calls%d/%d byte%d\n",k,op,n,calls,i);return 1;}
 }
 puts("8192 whole OID comparisons passed (4096 single,4096 all18)");return 0;
}
