#include "../cgame/cg_local.h"
#include "../cgame/tce_lightgrid.h"
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include <setjmp.h>
static unsigned char *data;
static int dataLength,position,originalMode,opened,checks;
static jmp_buf done;
static void require(int ok,const char *why){if(!ok){fprintf(stderr,"Grid loader: %s\n",why);exit(1);}}
void QDECL CG_Printf(const char *fmt,...) {(void)fmt;}
static void noop(void) {}
int trap_FS_FOpenFile(const char *path,fileHandle_t *file,fsMode_t mode) {
    require(mode==FS_READ,"open mode");position=0;*file=data?1:0;opened=data!=NULL;return data?dataLength:-1;
}
void trap_FS_Read(void *out,int count,fileHandle_t file) {
    int n=count;require(opened&&file==1&&count>=0,"read contract");
    if(n>dataLength-position)n=dataLength-position;
    if(!originalMode)require(n==count,"new loader overread");
    if(n>0)memcpy(out,data+position,n);position+=n;
}
void trap_FS_FCloseFile(fileHandle_t file) {
    require(opened&&file==1,"close contract");opened=0;
    if(originalMode)longjmp(done,1);
}
typedef struct {unsigned char *address,saved[5];} hook_t;
static void hook(hook_t *h,unsigned char *base,int offset,void *replacement) {
    DWORD protection,unused;h->address=base+offset;memcpy(h->saved,h->address,5);
    require(VirtualProtect(h->address,5,PAGE_EXECUTE_READWRITE,&protection),"hook protection");
    h->address[0]=0xe9;*(int *)(h->address+1)=(unsigned char *)replacement-h->address-5;
    FlushInstructionCache(GetCurrentProcess(),h->address,5);
    require(VirtualProtect(h->address,5,protection,&unused),"hook restore");
}
static void unhook(hook_t *h) {
    DWORD protection,unused;
    require(VirtualProtect(h->address,5,PAGE_EXECUTE_READWRITE,&protection),"unhook protection");
    memcpy(h->address,h->saved,5);FlushInstructionCache(GetCurrentProcess(),h->address,5);
    require(VirtualProtect(h->address,5,protection,&unused),"unhook restore");
}


static void rejected(const char *why) {
    require(!TCE_CG_LoadLightGrid("fixture"),why);
    require(!TCE_CG_MapLightGrid(),"stale grid exposed");require(!opened,"leaked handle");++checks;
}
int main(int argc,char **argv) {
    const char *names[]={"obj_delta","obj_northport","obj_railhouse","obj_snow","obj_stadtrand","obj_village"};
    const int nops[]={0x3ebf0,0x65ac0,0x02a60,0x03520,0x011e0,0x64ac0,0x47d70,0x47b10};
    hook_t hooks[11];HMODULE dll;unsigned char *base;char path[1024];FILE *f;
    const tce_lightGrid_t *grid;int i,j,n,backup[36],size;float modelBackup;int *header;
    void (__cdecl *original)(void);
    require(argc==3,"arguments");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"load DLL");base=(unsigned char *)dll;original=(void *)(base+0x49b40);
    for(i=0;i<8;++i)hook(&hooks[i],base,nops[i],noop);
    hook(&hooks[8],base,0x64ca0,trap_FS_FOpenFile);hook(&hooks[9],base,0x64cc0,trap_FS_Read);hook(&hooks[10],base,0x64d00,trap_FS_FCloseFile);
    for(n=0;n<6;++n) {
        snprintf(path,sizeof(path),"%s/%s.bsp",argv[2],names[n]);f=fopen(path,"rb");require(f!=NULL,"fixture BSP missing");
        fseek(f,0,SEEK_END);dataLength=(int)ftell(f);rewind(f);data=malloc(dataLength);require(data!=NULL,"fixture allocation");require(fread(data,1,dataLength,f)==dataLength,"fixture read");fclose(f);
        header=(int *)data;memcpy(backup,header,sizeof(backup));
        VectorSet(tce_lightGridSpacing,32,32,48);memcpy(base+0x40a4c24,tce_lightGridSpacing,12);
        originalMode=1;if(!setjmp(done))original();originalMode=0;
        require(TCE_CG_LoadLightGrid(path),"real BSP rejected");grid=TCE_CG_MapLightGrid();require(grid!=NULL,"missing grid");
        require(!memcmp(grid->origin,base+0x40a4c58,12),"origin differs from original");
        require(!memcmp(grid->bounds,base+0x40a4c30,12),"bounds differ from original");
        size=*(int *)(base+0x40a4c3c)*8;require(size==header[33],"original grid size mismatch");
        require(!memcmp(grid->samples,base+0x40a4c64,size),"loaded sample bytes differ from original");
        require(!opened,"success leaked handle");++checks;
        printf("%s: original grid match (%d bytes)\n",names[n],size);
        /* Mutations affect this in-memory fixture only, never pak. */
        header[1]=46;rejected("wrong version");memcpy(header,backup,sizeof(backup));
        header[0]=0;rejected("wrong magic");memcpy(header,backup,sizeof(backup));
        header[32]=-1;rejected("negative offset");memcpy(header,backup,sizeof(backup));
        header[32]=dataLength;rejected("out-of-file grid");memcpy(header,backup,sizeof(backup));
        header[33]-=8;rejected("wrong sample count");memcpy(header,backup,sizeof(backup));
        header[17]=0;rejected("missing world model");memcpy(header,backup,sizeof(backup));
        tce_lightGridSpacing[0]=0;rejected("zero spacing");tce_lightGridSpacing[0]=32;
        tce_lightGridSpacing[0]=-32;rejected("negative spacing");tce_lightGridSpacing[0]=32;
        tce_lightGridSpacing[0]=0.00001f;rejected("excessive count");tce_lightGridSpacing[0]=32;
        modelBackup=*(float *)(data+header[16]);*(unsigned int *)(data+header[16])=0x7fc00000;rejected("NaN model");*(float *)(data+header[16])=modelBackup;
        j=dataLength;dataLength=143;rejected("short header");dataLength=j;
        require(TCE_CG_LoadLightGrid(path),"reload after rejection");++checks;
        free(data);data=NULL;rejected("missing BSP");
    }
    for(i=10;i>=0;--i)unhook(&hooks[i]);FreeLibrary(dll);
    printf("%d loader checks, including six original load-path comparisons, passed.\n",checks);return 0;
}
