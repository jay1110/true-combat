#define WIN32_LEAN_AND_MEAN
#define NOGDI
#if defined(UIDLL)
#include "../ui/ui_local.h"
#elif defined(CGAMEDLL)
#include "../cgame/cg_local.h"
#else
#include "../game/g_main.c"
#endif
#include <windows.h>
#include <stdio.h>
typedef struct {vmCvar_t *vm;char *name,*value;int flags,modified;} row_t;
typedef struct {vmCvar_t *vm;char *name,*value;int flags,modified,track,reset,shader;} grow_t;
#ifdef GAMEDLL
extern void AddLean(gentity_t *,vec3_t);
extern void G_TCEUserinfoOptions(playerState_t *,const char *);
#else
extern row_t cvarTable[];extern int cvarTableSize;
#endif
static int calls;static char info[1024],events[8192];
static void patch(void *address,void *fn){unsigned char *p=address;DWORD old,tmp;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static void reg(vmCvar_t *v,const char *n,const char *d,int f){++calls;if(v){memset(v,0,sizeof(*v));v->modificationCount=3;v->integer=atoi(d);v->value=(float)atof(d);Q_strncpyz(v->string,d,sizeof(v->string));}}
static void set(const char *n,const char *v){strcat(events,n);strcat(events,"=");strcat(events,v);strcat(events,";");}
static void command(const char *v){set("command",v);}
static void update(vmCvar_t *v){}
static void var(const char *n,char *v,int size){Q_strncpyz(v,"1",size);}
static void getinfo(int n,char *v,int size){Q_strncpyz(v,info,size);}
static void getconfig(int n,char *v,int size){*v=0;}
static void config(int n,const char *v){}
static void noop(void){}
static int milliseconds(void){return 1234;}
#ifdef CGAMEDLL
extern void CG_DrawDemoRecording(void);
static void textSink(float x,float y,float sx,float sy,vec4_t color,const char *s,float adjust,int limit,int style,fontInfo_t *font){
 char data[1024];sprintf(data,"%g,%g,%g,%g,%g,%g,%g,%g,%g,%d,%d:%s",x,y,sx,sy,color[0],color[1],color[2],color[3],adjust,limit,style,s);set("text",data);
}
#endif
int main(int argc,char **argv){
 unsigned char *b;HMODULE h;int count,stride,table,i,j,found,cases=0,actual;
 char originalEvents[8192];
 if(argc!=2 || !(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
#ifdef UIDLL
 table=0x33260;stride=20;count=196;actual=cvarTableSize;
 patch(b+0x204f0,reg);patch(trap_Cvar_Register,reg);patch(b+0x20530,set);patch(trap_Cvar_Set,set);
 calls=0;((void (__cdecl *)(void))(b+0x12480))();if(calls!=count)return 3;strcpy(originalEvents,events);events[0]=0;
 calls=0;UI_RegisterCvars();if(calls!=actual||strcmp(events,originalEvents))return 3;
 patch(b+0x20510,update);patch(trap_Cvar_Update,update);
#elif defined(CGAMEDLL)
 table=0xaa3c8;stride=20;count=187;actual=cvarTableSize;
 patch(b+0x64bd0,reg);patch(trap_Cvar_Register,reg);patch(b+0x64c10,set);patch(trap_Cvar_Set,set);
 patch(b+0x64c30,var);patch(trap_Cvar_VariableStringBuffer,var);
 calls=0;((void (__cdecl *)(void))(b+0x476d0))();if(calls!=count)return 3;strcpy(originalEvents,events);events[0]=0;
 calls=0;CG_RegisterCvars();if(calls!=actual||strcmp(events,originalEvents))return 3;
 patch(b+0x64bf0,update);patch(trap_Cvar_Update,update);
 patch(b+0x64d60,command);patch(trap_SendConsoleCommand,command);
 patch(b+0x64bc0,milliseconds);patch(trap_Milliseconds,milliseconds);
#else
 table=0xc0a58;stride=32;count=156;actual=gameCvarTableSize;
#endif
 for(i=0;i<count;i++) {
  row_t *r=(void *)(b+table+i*stride);found=0;
  /* PE relocations make the reference strings/pointers directly readable. */
  if(!*r->name)continue;
  for(j=0;j<actual;j++) {
#ifdef GAMEDLL
   row_t *p=(void *)&gameCvarTable[j];
#else
   row_t *p=&cvarTable[j];
#endif
   if(Q_stricmp(r->name,p->name))continue;found=1;
   if(strcmp(r->name,"gamename") && strcmp(r->name,"gamedate") && strcmp(r->name,"tce_version") &&
     (strcmp(r->value,p->value)||r->flags!=p->flags)) {printf("cvar mismatch %s: %s/%d vs %s/%d\n",r->name,r->value,r->flags,p->value,p->flags);return 1;}
#ifdef GAMEDLL
   if(memcmp((char *)r+20,(char *)p+20,12)){printf("reset/track mismatch %s\n",r->name);return 1;}
#endif
#ifndef GAMEDLL
   if(p->vm && p->modified!=p->vm->modificationCount && Q_stricmp(p->name,"cg_errorDecay"))return 4;
   if(p->vm && r->vm) {
    int scenario;
    for(scenario=0;scenario<4;scenario++) {
     vmCvar_t *a=r->vm,*v=p->vm;const char *value;
     /* Exercise every registered VM's changed and unchanged update paths. */
     if(strstr(r->name,"Color"))value=scenario&1?"Red":"White";
     else if(strstr(r->name,"Password"))value=scenario&1?"example":"";
     else value=scenario==0?"0":scenario==1?"1":scenario==2?"501":"-1";
     a->modificationCount++;v->modificationCount++;
     a->integer=v->integer=atoi(value);a->value=v->value=(float)atof(value);
     strcpy(a->string,value);strcpy(v->string,value);events[0]=0;
#ifdef UIDLL
     ((void (__cdecl *)(void))(b+0x12510))();
     strcpy(originalEvents,events);events[0]=0;UI_UpdateCvars();
#else
     ((void (__cdecl *)(void))(b+0x47800))();
     strcpy(originalEvents,events);events[0]=0;CG_UpdateCvars();
#endif
     if(strcmp(events,originalEvents)){printf("update mismatch %s/%d: %s vs %s\n",r->name,scenario,originalEvents,events);return 1;}
     if(p->modified!=v->modificationCount || r->modified!=a->modificationCount)return 1;
     ++cases;
    }
   }
#endif
   ++cases;break;
  }
  if(!found){printf("missing cvar %s\n",r->name);return 1;}
 }
 #ifdef CGAMEDLL
 patch(b+0x20ba0,textSink);patch(CG_Text_Paint_Ext,textSink);
 for(i=0;i<64;i++) {
  extern vmCvar_t cl_demorecording,cl_waverecording,cl_demofilename,cl_wavefilename,cl_demooffset,cl_waveoffset,cg_recording_statusline;
  cg_recording_showstatusline.integer=*(int *)(b+0x488a0ec)=i&1;
  cl_demorecording.integer=*(int *)(b+0x488ad4c)=(i>>1)&1;
  cl_waverecording.integer=*(int *)(b+0x40130ac)=(i>>2)&1;
  cg_recording_statusline.integer=*(int *)(b+0x4010eec)=(i>>3)*9;
  cl_demooffset.integer=*(int *)(b+0x4b32b6c)=i*1025-2048;
  cl_waveoffset.integer=*(int *)(b+0x484766c)=i*981-4096;
  strcpy(cl_demofilename.string,"cvar_demo");strcpy((char *)b+0x488d4b0,"cvar_demo");
  strcpy(cl_wavefilename.string,"cvar_wave");strcpy((char *)b+0x2575f90,"cvar_wave");
  events[0]=0;((void (__cdecl *)(void))(b+0x21b30))();strcpy(originalEvents,events);events[0]=0;CG_DrawDemoRecording();
  if(strcmp(events,originalEvents)){printf("recording mismatch %d: %s vs %s\n",i,originalEvents,events);return 1;}++cases;
 }
 #endif
#ifdef GAMEDLL
 {
  static unsigned char originalEnt[0x57c],originalClient[0x16b0];
  gentity_t ent;gclient_t client;vec3_t a,v;int mode,k;
  memset(&ent,0,sizeof(ent));memset(&client,0,sizeof(client));ent.client=&client;
  *(void **)(originalEnt+0x190)=originalClient;
  for(mode=-1;mode<3;mode++)for(i=0;i<257;i++){
   g_leanmode.integer=*(int *)(b+0x23cd72c)=mode;
   client.ps.leanf=*(float *)(originalClient+0x3c)=(i-128)*.1875f;
   for(k=0;k<3;k++){client.ps.viewangles[k]=((i*7+k*19)%360)-180.0f;((float *)(originalClient+0xb0))[k]=client.ps.viewangles[k];a[k]=v[k]=i-k*17.0f;}
   ((void (__cdecl *)(void *,float *))(b+0x9f400))(originalEnt,a);AddLean(&ent,v);
   for(k=0;k<3;k++)if(fabs(a[k]-v[k])>.00005f){printf("lean mismatch %d/%d: %g %g\n",mode,i,a[k],v[k]);return 1;}++cases;
  }
  ent.client=NULL;a[0]=1;a[1]=2;a[2]=3;AddLean(&ent,a);if(a[0]!=1||a[1]!=2||a[2]!=3)return 1;
  patch(b+0x8ae00,getinfo);patch(b+0x8ade0,getconfig);patch(b+0x8adc0,config);
  patch(b+0x5ed80,noop);patch(b+0x62600,noop);patch(b+0x5edc0,noop);patch(b+0x8ad60,noop);
  *(void **)(b+0x226e430)=originalClient;
  for(i=0;i<128;i++)for(j=0;j<8;j++){
   int seed=(i*0x10101)|0x1c;
   memset(originalClient,0,sizeof(originalClient));*(int *)(originalClient+0x148)=seed;
   memset(&client,0,sizeof(client));client.ps.persistant[14]=seed;
   sprintf(info,"\\name\\test\\cg_uinfo\\0 0 30\\cg_toggleCrouch\\%d\\cg_toggleAiming\\%d\\cg_freeAim\\%d",j&1?2:0,j&2?-1:0,j&4?1:0);
   ((void (__cdecl *)(int))(b+0x4bf70))(0);G_TCEUserinfoOptions(&client.ps,info);
   if(client.ps.persistant[14]!=*(int *)(originalClient+0x148)){printf("userinfo mismatch\n");return 1;}++cases;
  }
 }
#endif
 printf("%d original cvar/consumer comparisons passed (%d registrations)\n",cases,actual);return 0;
}
