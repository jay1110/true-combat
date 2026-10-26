#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_local.h"
#include <windows.h>
#include <stdio.h>
extern void UI_Update(const char *name);
static char events[16384], payload[2048];
static float value, rate;
static void patch(void *a,void *f){DWORD o,t;unsigned char *p=a;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void set(const char *n,const char *v){sprintf(events+strlen(events),"set[%s]=[%s];",n,v);}
static void setvalue(const char *n,float v){unsigned bits;memcpy(&bits,&v,4);sprintf(events+strlen(events),"float[%s]=%08x;",n,bits);}
static float getvalue(const char *n){sprintf(events+strlen(events),"get[%s];",n);return !Q_stricmp(n,"ui_rate")?rate:value;}
static void getstring(const char *n,char *out,int size){sprintf(events+strlen(events),"string[%s],%d;",n,size);Q_strncpyz(out,payload,size);}
static void command(int when,const char *s){sprintf(events+strlen(events),"command[%d]=[%s];",when,s);}
int main(int argc,char **argv){
 HMODULE h;unsigned char *b;int i,j,k,cases=0,step;char expected[16384],mixed[128];
 const char *names[]={"ui_SetName","ui_setRate","ui_GetName","r_colorbits","ui_r_lodbias","ui_glCustom","ui_mousePitch","unknown",""};
 const float values[]={-5000,-1.9f,-1,-.9f,0,.9f,1,1.9f,2,2.9f,3,3.9f,4,15.9f,16,16.9f,31.9f,32,32.9f,4000,4999.5f,5000};
 const float rates[]={-1,0,3999.5f,4000,4000.5f,4999.5f,5000,5000.5f,25000};
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
 patch(b+0x20530,set);patch(trap_Cvar_Set,set);patch(b+0x20550,getvalue);patch(trap_Cvar_VariableValue,getvalue);
 patch(b+0x20570,getstring);patch(trap_Cvar_VariableStringBuffer,getstring);patch(b+0x20590,setvalue);patch(trap_Cvar_SetValue,setvalue);
 patch(b+0x205d0,command);patch(trap_Cmd_ExecuteText,command);
 for(i=0;i<9;++i)for(j=0;j<22;++j)for(k=0;k<9;++k){
  value=values[j];rate=rates[k];Q_strncpyz(mixed,names[i],sizeof(mixed));if(k&1)Q_strupr(mixed);sprintf(payload,"^1Player name %d/%d",j,k);
  events[0]=0;((void(__cdecl *)(const char *))(b+0x6690))(mixed);strcpy(expected,events);events[0]=0;UI_Update(mixed);
  if(strcmp(events,expected)){printf("dispatch mismatch %s value=%g rate=%g\n%s\n%s\n",mixed,value,rate,expected,events);return 1;}++cases;
 }
 /* Compare the observable two-call buffer lifetime, not relocated pointers. */
 for(i=0;i<8;++i){
  const int lengths[]={0,1,31,1022,1023,1024,1536,2047};char *original[3],*actual[3];
  for(step=0;step<3;++step){int len=lengths[(i+step)%8];memset(payload,'a'+step,len);payload[len]=0;
   events[0]=0;original[step]=((char *(__cdecl *)(const char *))(b+0x23e0))("buffer_test");strcpy(expected,events);events[0]=0;actual[step]=UI_Cvar_VariableString("buffer_test");
   if(strcmp(events,expected)||strcmp(original[step],actual[step]))return 3;
   for(k=0;k<=step;++k)if(strcmp(original[k],actual[k]))return 4;
   ++cases;
  }
  if(original[0]!=original[2]||actual[0]!=actual[2]||original[0]==original[1]||actual[0]==actual[1])return 5;
 }
 printf("%d complete Windows UI cvar dispatch/string-buffer comparisons passed\n",cases);return 0;
}
