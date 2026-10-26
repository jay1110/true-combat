#include "../cgame/cg_local.h"
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#undef _vsnprintf
#undef vsnprintf

gitem_t bg_itemlist[MAX_ITEMS];
itemInfo_t cg_items[MAX_ITEMS];
static int count, mode, visualCases, soundCases;
typedef struct {int kind, isNull, compressed;char name[512];} call_t;
static call_t calls[32], expected[32];
static void require(int ok,const char *what) {
    if(!ok){fprintf(stderr,"Item media visual=%d sound=%d: %s\n",visualCases,soundCases,what);exit(1);}
}
static int record(int kind,const char *name,int compressed) {
    call_t *c;require(count<32,"call overflow");c=&calls[count++];memset(c,0,sizeof(*c));
    c->kind=kind;c->isNull=name==NULL;c->compressed=compressed;
    if(name){require(strlen(name)<sizeof(c->name),"path overflow");strcpy(c->name,name);}
    return mode==0?100+count:mode==1?0:-count;
}
qhandle_t trap_R_RegisterModel(const char *name){return record(1,name,0);}
qhandle_t trap_R_RegisterShader(const char *name){return record(2,name,0);}
sfxHandle_t trap_S_RegisterSound(const char *name,qboolean compressed){return record(3,name,compressed);}
void QDECL CG_Error(const char *format,...) {
    char text[512];va_list args;va_start(args,format);_vsnprintf(text,sizeof(text),format,args);va_end(args);record(4,text,0);
}
/* Formatting support only; original va runs unmodified in the DLL. */
char * QDECL va(char *format,...) {
    static char text[1024];va_list args;va_start(args,format);_vsnprintf(text,sizeof(text),format,args);va_end(args);return text;
}
typedef struct {unsigned char *address,saved[5];} hook_t;
static void hook(hook_t *h,unsigned char *base,int offset,void *replacement) {
    DWORD p,u;h->address=base+offset;memcpy(h->saved,h->address,5);
    require(VirtualProtect(h->address,5,PAGE_EXECUTE_READWRITE,&p),"hook protection");
    h->address[0]=0xe9;*(int *)(h->address+1)=(unsigned char *)replacement-h->address-5;
    FlushInstructionCache(GetCurrentProcess(),h->address,5);VirtualProtect(h->address,5,p,&u);
}
static void unhook(hook_t *h) {
    DWORD p,u;require(VirtualProtect(h->address,5,PAGE_EXECUTE_READWRITE,&p),"unhook");
    memcpy(h->address,h->saved,5);FlushInstructionCache(GetCurrentProcess(),h->address,5);VirtualProtect(h->address,5,p,&u);
}
static void compare(unsigned char *base,int index,const gitem_t *item,int registered,int sound) {
    unsigned char *table=base+0x98f58,*media=base+0x48478a0;
    unsigned char savedItem[0x3c];gitem_t savedPortItem;
    void (__cdecl *original)(int)=(void *)(base+(sound?0x49a40:0x6f280));
    int n;
    memcpy(savedItem,table+index*0x3c,sizeof(savedItem));
    memcpy(table+index*0x3c,item,0x3c);bg_itemlist[index]=*item;savedPortItem=*item;
    memset(cg_items,0x5a,sizeof(cg_items));cg_items[index].registered=registered;
    memcpy(media,cg_items,sizeof(cg_items));
    count=0;memset(calls,0,sizeof(calls));original(index);n=count;memcpy(expected,calls,sizeof(calls));
    count=0;memset(calls,0,sizeof(calls));
    if(sound)CG_RegisterItemSounds(index);else CG_RegisterItemVisuals(index);
    require(count==n&&!memcmp(calls,expected,sizeof(calls)),"registration order/names/errors/flags");
    require(!memcmp(cg_items,media,sizeof(cg_items)),"all media words including untouched slots");
    require(!memcmp(&bg_itemlist[index],&savedPortItem,sizeof(savedPortItem)),"port item mutation");
    require(!memcmp(table+index*0x3c,item,0x3c),"original item mutation");
    /* Cached visual calls must remain no-ops; weapon slots stay unregistered. */
    if(!sound){
        count=0;memset(calls,0,sizeof(calls));original(index);n=count;memcpy(expected,calls,sizeof(calls));
        count=0;memset(calls,0,sizeof(calls));CG_RegisterItemVisuals(index);
        require(count==n&&!memcmp(calls,expected,sizeof(calls)),"repeat registration");
        require(!memcmp(cg_items,media,sizeof(cg_items)),"repeat media");
        ++visualCases;
    }else ++soundCases;
    memcpy(table+index*0x3c,savedItem,sizeof(savedItem));
}
int main(int argc,char **argv) {
    static gitem_t originals[256];
    const char *paths[]={NULL,"","models/test.md3"};
    const char *icons[]={NULL,"","icons/item","icons/item.tga"};
    const char *sounds[]={NULL,"","a.wav","abwav","a.WAV","a.wavx","a.ogg","a.wav b.wav","a.wav "," a.wav","a.wav  b.wav","a.wav x","a\tb.wav","a.wav\nb.wav"};
    const int registered[]={0,1,-1},indices[]={0,1,17,92};
    HMODULE dll;unsigned char *base;hook_t hooks[4];DWORD protection,unused;
    int n,i,j,k,l,a,b,c;gitem_t item;char generated[180];
    require(argc==2,"DLL argument");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"load original");base=(unsigned char *)dll;
    n=*(int *)(base+0x9a560);require(n==93,"original item count");memcpy(originals,base+0x98f58,n*0x3c);
    require(VirtualProtect(base+0x98f58,n*0x3c,PAGE_READWRITE,&protection),"item fixtures protection");
    hook(&hooks[0],base,0x659f0,trap_R_RegisterModel);hook(&hooks[1],base,0x65a30,trap_R_RegisterShader);
    hook(&hooks[2],base,0x659d0,trap_S_RegisterSound);hook(&hooks[3],base,0x47bf0,CG_Error);
    /* Every real TC:E item: no copied asset data or production table changes. */
    for(i=0;i<n;++i)for(mode=0;mode<3;++mode)for(j=0;j<3;++j){compare(base,i,&originals[i],registered[j],0);compare(base,i,&originals[i],registered[j],1);}
    memset(&item,0,sizeof(item));item.classname="test_item";
    for(mode=0;mode<3;++mode)for(i=0;i<10;++i)for(j=0;j<4;++j)for(k=0;k<3;++k)
    for(a=0;a<3;++a)for(b=0;b<3;++b)for(c=0;c<3;++c) {
        item.giType=i;item.icon=(char *)icons[j];item.world_model[0]=(char *)paths[a];item.world_model[1]=(char *)paths[b];item.world_model[2]=(char *)paths[c];
        compare(base,indices[(i+j+a+b+c)%4],&item,registered[k],0);
    }
    for(mode=0;mode<3;++mode)for(i=0;i<3;++i)for(j=0;j<14;++j)for(k=0;k<4;++k){
        item.pickup_sound=(char *)paths[i];item.sounds=(char *)sounds[j];compare(base,indices[k],&item,0,1);
    }
    /* Exact token-length boundaries plus bad tokens after a valid sound. */
    for(mode=0;mode<3;++mode)for(i=0;i<=80;++i)for(j=0;j<4;++j)for(k=0;k<3;++k){
        char *token=generated;memset(generated,0,sizeof(generated));
        if(j&1){strcpy(generated,"first.wav ");token+=strlen(generated);}
        memset(token,'a',i);token[i]=0;if(i>=3)memcpy(token+i-3,j&2?"WAV":"wav",3);
        item.pickup_sound=(char *)paths[k];item.sounds=generated;compare(base,indices[j],&item,0,1);
    }
    require(!memcmp(originals,base+0x98f58,n*0x3c),"original table restored");
    for(l=3;l>=0;--l)unhook(&hooks[l]);VirtualProtect(base+0x98f58,n*0x3c,protection,&unused);FreeLibrary(dll);
    printf("%d item-visual and %d item-sound original comparisons passed; all 93 original items included.\n",visualCases,soundCases);return 0;
}
