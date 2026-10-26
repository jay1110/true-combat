#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_character.c"
#include <windows.h>
#include <stdio.h>
/* Private registration callees above remain addressable for boundary hooks. */
#include "tce_character_pool_checks.h"
static int seed,original; static char events[16384]; static bg_characterDef_t def;
static void patch(void *a,void *f){DWORD o,t;unsigned char *p=a;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static unsigned hash(const char *s){unsigned h=5381;while(*s)h=h*33+(unsigned char)*s++;return h;}
static void logText(const char *tag,const char *s){strcat(events,tag);strcat(events,s);strcat(events,";");}
static const char *config(int n){strcat(events,va("config:%d;",n));return seed&1?"\\camo\\3":"";}
static qboolean parse(const char *f,bg_characterDef_t *d){logText("parse:",f);memcpy(d,&def,sizeof(def));return (seed%19)!=18;}
static int model(const char *f){logText("model:",f);return (seed%17)==16?0:hash(f)%10000+1;}
static int skin(const char *f){unsigned h=hash(f);logText("skin:",f);return ((h^(seed*31))%5)==0 || seed==127 ? 0:h%10000+1;}
static qboolean getSkinModel(int h,const char *type,char *out){logText("accessory:",type);if((hash(type)+seed)%3==0)return qfalse;sprintf(out,"models/acc/%s.md3",type);return qtrue;}
static void QDECL warning(const char *fmt,...){char s[2048];va_list ap;va_start(ap,fmt);vsprintf(s,fmt,ap);va_end(ap);logText("warning:",s);}
static qboolean headConfig(const char *f,animation_t *a){logText("headanim:",f);memset(a,0x33,sizeof(animation_t)*MAX_HD_ANIMATIONS);return seed%7!=6;}
static qboolean existing(const char *g,const char *s,animModelInfo_t **a){logText("group:",g);logText("script:",s);*a=(animModelInfo_t *)0x1234;return seed&2;}
static qboolean animFiles(bg_character_t *c,const char *g,const char *s){logText("animfiles:",g);return seed%11!=10;}
static void simpleSave(void){strcat(events,"save;");}static void simpleRestore(void){strcat(events,"restore;");}static void clear(void){strcat(events,"clear;");}
static void adjust(float *x,float *y,float *w,float *h){*x*=1.25f;*y*=1.5f;*w*=1.25f;*h*=1.5f;}
static void bounds(int m,vec3_t mins,vec3_t maxs){VectorSet(mins,-10,-7,-12);VectorSet(maxs,9,8,24);}
static void hudAnim(bg_character_t *c,void *lf,int *old,int *frame,float *back,int anim){strcat(events,"hudanim;");*old=13;*frame=17;*back=.7f;}
static void tag(refEntity_t *c,const refEntity_t *p,const char *n,int i,vec3_t o){logText("tag:",n);VectorCopy(p->origin,c->origin);}
static refEntity_t entities[4];static refdef_t scene;static int entityCount;
static void add(const refEntity_t *e){entities[entityCount++]=*e;strcat(events,"add;");}
static void render(const refdef_t *r){scene=*r;strcat(events,"render;");}
static int floatEqual(float a,float b){return fabs(a-b)<.0001;}
static bg_character_t panelCharacter;
static int panelTeam(void){return seed%5;}
static bg_character_t *panelChar(void){strcat(events,"character;");return &panelCharacter;}
static void panelHead(rectDef_t *r,void *c,void *h,float yaw,float pitch,int hat,int anim,int pain,int rank){strcat(events,va("head:%.2f:%.2f:%.2f:%.2f:%.2f:%.2f:%d:%d:%d:%d;",r->x,r->y,r->w,r->h,yaw,pitch,hat,anim,pain,rank));}
static void color(const float *c){if(c)strcat(events,va("color:%.3f:%.3f:%.3f:%.3f;",c[0],c[1],c[2],c[3]));else strcat(events,"color:null;");}
static void fill(float x,float y,float w,float h,const float *c){strcat(events,va("fill:%.2f:%.2f:%.2f:%.2f;",x,y,w,h));color(c);}
static void pic(float x,float y,float w,float h,int shader){strcat(events,va("pic:%.2f:%.2f:%.2f:%.2f:%d;",x,y,w,h,shader));}
static void picst(float x,float y,float w,float h,float s,float t,float u,float v,int shader){pic(x,y,w,h,shader);strcat(events,va("uv:%.2f:%.2f:%.2f:%.2f;",s,t,u,v));}
int main(int argc,char **argv){HMODULE h;unsigned char *b;int n,i,r1,r2;char expected[16384];bg_character_t c,rc;refEntity_t re[4];refdef_t rs;int rn;rectDef_t rect={12,18,43,56};
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
 printf("character offsets mesh %x acc %x skins %x corpse %x head %x anim %x damage %x info %x sizeofanim %u\n",offsetof(bg_character_t,mesh),offsetof(bg_character_t,accModels),offsetof(bg_character_t,accSkins),offsetof(bg_character_t,undressedCorpseModel),offsetof(bg_character_t,hudhead),offsetof(bg_character_t,hudheadanimations),offsetof(bg_character_t,bodyDamageSkins),offsetof(bg_character_t,animModelInfo),(unsigned)sizeof(animation_t));
 n=checkCharacterPool(b,1);if(n<0){printf("pool failure %d\n",n);return 3;}printf("cgame pool/getter: %d comparisons\n",n);
 patch(b+0x481a0,config);patch(CG_ConfigString,config);patch(b+0x3050,parse);patch(BG_ParseCharacterFile,parse);
 patch(b+0x659f0,model);patch(trap_R_RegisterModel,model);patch(b+0x65a10,skin);patch(trap_R_RegisterSkin,skin);
 patch(b+0x65240,getSkinModel);patch(trap_R_GetSkinModel,getSkinModel);patch(b+0x47b10,warning);patch(CG_Printf,warning);
 patch(b+0x18be0,headConfig);patch(CG_ParseHudHeadConfig,headConfig);patch(b+0x19020,existing);patch(CG_CheckForExistingAnimModelInfo,existing);
 patch(b+0x18d40,animFiles);patch(CG_ParseAnimationFiles,animFiles);
 for(seed=0;seed<256;++seed){memset(&def,0,sizeof(def));strcpy(def.mesh,"models/body.mdm");strcpy(def.skin,"s");strcpy(def.skinGroup,"temperate");strcpy(def.skinRoot,"allied/sniper/body");strcpy(def.animationGroup,"animations/base.anim");strcpy(def.animationScript,"animations/base.script");
  if(seed&4){strcpy(def.hudhead,"models/head.md3");if(seed&8)strcpy(def.hudheadskin,"models/head_default.skin");if(seed&16)strcpy(def.hudheadanims,"heads/base.anim");}
  if(seed&32){strcpy(def.undressedCorpseModel,"models/naked.mdm");strcpy(def.undressedCorpseSkin,"naked");}
  /* Original34bab600 is gearDef.playerSkinGroup (+0x1700), cache1443. */
  strcpy(gearDef.playerSkinGroup,seed&64?"winter":"");strcpy((char *)b+0x4bab600,gearDef.playerSkinGroup);
  memset(&c,0x44,sizeof(c));memset(&rc,0x44,sizeof(rc));events[0]=0;r1=((int(__cdecl *)(const char *,void *))(b+0x18460))("test.char",&rc);strcpy(expected,events);
  events[0]=0;r2=CG_RegisterCharacter("test.char",&c);
  if(r1!=r2||strcmp(expected,events)||memcmp(&c,&rc,sizeof(c))){printf("register mismatch seed %d result %d/%d\nEXPECTED %s\nACTUAL %s\n",seed,r1,r2,expected,events);for(i=0;i<sizeof(c);++i)if(((unsigned char *)&c)[i]!=((unsigned char *)&rc)[i]){printf("first field byte %x\n",i);break;}return 4;}
 }
 patch(b+0x65400,simpleSave);patch(trap_R_SaveViewParms,simpleSave);patch(b+0x65410,simpleRestore);patch(trap_R_RestoreViewParms,simpleRestore);
 patch(b+0x29760,adjust);patch(CG_AdjustFrom640,adjust);patch(b+0x65280,clear);patch(trap_R_ClearScene,clear);
 patch(b+0x654f0,bounds);patch(trap_R_ModelBounds,bounds);patch(b+0x56870,hudAnim);patch(CG_HudHeadAnimation,hudAnim);
 patch(b+0x65290,add);patch(trap_R_AddRefEntityToScene,add);patch(b+0x653e0,render);patch(trap_R_RenderScene,render);
 /* PositionEntityOnTag's shared tested implementation is replaced only at the renderer boundary. */
 patch(CG_PositionEntityOnTag,tag);patch(b+0x2de20,tag);
 for(seed=0;seed<64;++seed){memset(&c,0,sizeof(c));c.hudhead=11;c.hudheadskin=12;c.accModels[ACC_HAT]=13;c.accSkins[ACC_HAT]=14;c.accModels[ACC_RANK]=15;rankicons[2][1].shader=*(int *)(b+0xb31e0+2*32)=16;
  cg.time=*(int *)(b+0x407dea4)=12345;gearDef.playerSkinGroup[0]=0;cg.showGameView=*(int *)(b+0x4081fa8)=seed&1;
  entityCount=0;events[0]=0;memset(&scene,0,sizeof(scene));
  ((void(__cdecl *)(void *,void *,void *,float,float,int,int,int,int))(b+0x41270))(&rect,seed&32?NULL:&c,&c,150.f,12.f,seed&4,3,seed&2?23:0,seed&8?2:0);
  strcpy(expected,events);rn=entityCount;memcpy(re,entities,sizeof(re));rs=scene;
  entityCount=0;events[0]=0;memset(&scene,0,sizeof(scene));CG_DrawPlayerHead(&rect,seed&32?NULL:&c,&c,150,12,seed&4,3,seed&2?23:0,seed&8?2:0,qfalse);
  if(strcmp(expected,events)||rn!=entityCount||memcmp(&rs,&scene,sizeof(scene))){printf("head scene mismatch %d\n",seed);return 5;}
  for(i=0;i<rn;++i){int k;for(k=0;k<3;++k)if(!floatEqual(re[i].origin[k],entities[i].origin[k])){printf("head origin mismatch %d %.8f %.8f\n",k,re[i].origin[k],entities[i].origin[k]);return 6;}for(k=0;k<9;++k)if(!floatEqual(((float *)re[i].axis)[k],((float *)entities[i].axis)[k]))return 8;memset(re[i].axis,0,sizeof(re[i].axis));memset(entities[i].axis,0,sizeof(entities[i].axis));VectorClear(re[i].origin);VectorClear(entities[i].origin);if(memcmp(re+i,entities+i,sizeof(refEntity_t))){printf("head entity mismatch %d\n",seed);for(k=0;k<sizeof(refEntity_t);++k)if(((unsigned char *)(re+i))[k]!=((unsigned char *)(entities+i))[k]){printf("byte %x original %02x source %02x\n",k,((unsigned char *)(re+i))[k],((unsigned char *)(entities+i))[k]);}return 7;}}
 }
 patch(b+0x41270,panelHead);patch(CG_DrawPlayerHead,panelHead);patch(b+0x434f0,panelTeam);patch(CG_LimboPanel_GetTeam,panelTeam);patch(b+0x43540,panelChar);patch(CG_LimboPanel_GetCharacter,panelChar);
 patch(b+0x29850,fill);patch(CG_FillRect,fill);patch(b+0x29ea0,pic);patch(CG_DrawPic,pic);patch(b+0x29e50,picst);patch(CG_DrawPicST,picst);patch(b+0x65420,color);patch(trap_R_SetColor,color);
 cgs.media.limboSpectator=*(int *)(b+0x259d174)=11;cgs.media.limboWeaponCardSurroundH=*(int *)(b+0x259d0bc)=12;cgs.media.limboWeaponCardSurroundV=*(int *)(b+0x259d0c0)=13;cgs.media.limboWeaponCardSurroundC=*(int *)(b+0x259d0c4)=14;
 for(seed=0;seed<40;++seed){panel_button_t button;memset(&button,0,sizeof(button));button.rect=rect;button.rect.x+=seed*.125f;
  events[0]=0;((void(__cdecl *)(void *))(b+0x41650))(&button);strcpy(expected,events);events[0]=0;CG_LimboPanel_RenderHead(&button);if(strcmp(expected,events)){printf("panel mismatch %d\n%s\n%s\n",seed,expected,events);return 9;}}
 printf("40 whole Limbo RenderHead comparisons passed\n");
 printf("256 whole character registrations and 64 head-render comparisons passed including hat/rank/pain/null branches\n");return 0;
}
