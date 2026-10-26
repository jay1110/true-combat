#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include "../cgame/tce_weapon_media.h"
#include <windows.h>
#include <stdio.h>
extern void CG_DrawBorder(float,float,float,float,qboolean,qboolean);
extern panel_button_text_t weaponPanelNameFont;
static unsigned char *base;
static char events[32768];
static int team,cls,count,selected,slot,hover,disabled;
static void *focus;
static void patch(void *a,void *f){DWORD o,t;unsigned char *p=a;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static int fontId(void *f){if(f==base+0x2588f8c||f==&cgs.media.limboFont1)return 1;if(f==base+0x2598058||f==&cgs.media.limboFont2)return 2;if(f==base+0x2593014||f==&cgs.media.limboWeaponCountFont)return 3;return -1;}
static int width(const char *s,float scale,int limit,void *f){strcat(events,va("width:%s:%.3f:%d:%d;",s,scale,limit,fontId(f)));return 17;}
static void text(float x,float y,float sx,float sy,const float *c,const char *s,float adjust,int limit,int style,void *f){strcat(events,va("text:%.3f,%.3f,%.3f,%.3f:%.3f,%.3f,%.3f,%.3f:%s:%.3f:%d:%d:%d;",x,y,sx,sy,c[0],c[1],c[2],c[3],s,adjust,limit,style,fontId(f)));}
static void pic(float x,float y,float w,float h,int shader){strcat(events,va("pic:%.3f,%.3f,%.3f,%.3f:%d;",x,y,w,h,shader));}
static void fill(float x,float y,float w,float h,const float *c){strcat(events,va("fill:%.3f,%.3f,%.3f,%.3f:%.3f,%.3f,%.3f,%.3f;",x,y,w,h,c[0],c[1],c[2],c[3]));}
static int inside(const rectDef_t *r){strcat(events,va("inside:%.3f,%.3f,%.3f,%.3f;",r->x,r->y,r->w,r->h));return hover==1||(hover==2&&r->y<200);}
static void color(const float *c){if(c)strcat(events,va("color:%.3f,%.3f,%.3f,%.3f;",c[0],c[1],c[2],c[3]));else strcat(events,"color:null;");}
static int getTeam(void){strcat(events,"team;");return team;}
static int getClass(void){strcat(events,"class;");return cls;}
static int getCount(void){strcat(events,"count;");return count;}
static int slotCount(int s){strcat(events,va("slotCount:%d;",s));return count;}
static int number(void){strcat(events,"number;");return selected;}
static int weapon(void){strcat(events,"weapon;");return 3+selected;}
static int forNumber(int n,int s,int ignore){strcat(events,va("forNumber:%d:%d:%d;",n,s,ignore));return 3+n;}
static int isDisabled(int w){strcat(events,va("disabled:%d;",w));return disabled;}
static void *playerClass(void){strcat(events,"playerClass;");return NULL;}
static void *getFocus(void){strcat(events,"focus;");return focus;}
static void border(float x,float y,float w,float h,int a,int b){strcat(events,va("border:%.3f,%.3f,%.3f,%.3f:%d:%d;",x,y,w,h,a,b));}
static void card(rectDef_t *r,int w,int hi,const char *s,int dis){strcat(events,va("card:%.3f,%.3f,%.3f,%.3f:%d:%d:%s:%d;",r->x,r->y,r->w,r->h,w,hi,s,dis));}
static void sound(int s,int ch){strcat(events,va("sound:%d:%d;",s,ch));}
static int strings(const char *a,const char *b){return (!a&&!b)||(a&&b&&!strcmp(a,b));}
#define HOOK(off,own,fn) patch(base+off,fn);patch(own,fn)
int main(int argc,char **argv){HMODULE h;int w,hi,n,old,f,expectedData,a,b,c,cases=0;char expected[32768];panel_button_t button;rectDef_t rect={642,250,179,45};weaponType_t *r,*s;
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;base=(unsigned char *)h;
 for(w=-3;w<75;++w){r=((weaponType_t *(__cdecl *)(int))(base+0x56660))(w);s=WM_FindWeaponTypeForWeapon(w);if((!r)!=(!s)||(r&&(r->weapindex!=s->weapindex||!strings(r->desc,s->desc)||!strings(r->description,s->description)||!strings(r->category,s->category)))){printf("weapon type mismatch %d\n",w);return 1;}++cases;}
 HOOK(0x20910,CG_Text_Width_Ext,width);HOOK(0x20ba0,CG_Text_Paint_Ext,text);HOOK(0x29ea0,CG_DrawPic,pic);HOOK(0x29850,CG_FillRect,fill);HOOK(0x87af0,BG_CursorInRect,inside);HOOK(0x65420,trap_R_SetColor,color);
 cgs.media.limboWeaponCard=*(int *)(base+0x259d0b4)=101;cgs.media.limboWeaponCardHighlight=*(int *)(base+0x259d0b8)=102;
 for(w=0;w<64;++w){strcpy(tce_cg_weapons[w].deployMenuShortName,"TC name");strcpy((char *)base+0x484af6c+w*0xfd0,"TC name");strcpy(tce_cg_weapons[w].deployMenuDescription,"TC description");strcpy((char *)base+0x484afac+w*0xfd0,"TC description");strcpy(tce_cg_weapons[w].deployMenuType,"TC type");strcpy((char *)base+0x484afec+w*0xfd0,"TC type");
 for(hi=0;hi<2;++hi)for(disabled=0;disabled<2;++disabled)for(hover=0;hover<3;++hover){events[0]=0;((void(__cdecl *)(void *,int,int,char *,int))(base+0x41c50))(&rect,w,hi,"2of5",disabled);strcpy(expected,events);events[0]=0;CG_LimboPanel_WeaponPanel_DrawWeapon(&rect,w,hi,"2of5",disabled);if(strcmp(expected,events)){printf("card mismatch w%d hi%d dis%d hover%d\n%s\n%s\n",w,hi,disabled,hover,expected,events);return 1;}++cases;}}
 HOOK(0x434f0,CG_LimboPanel_GetTeam,getTeam);HOOK(0x43530,CG_LimboPanel_GetClass,getClass);HOOK(0x43580,CG_LimboPanel_WeaponCount,getCount);HOOK(0x43590,CG_LimboPanel_WeaponCount_ForSlot,slotCount);
 HOOK(0x43ae0,CG_LimboPanel_GetSelectedWeaponNum,number);HOOK(0x43ac0,CG_LimboPanel_GetSelectedWeapon,weapon);HOOK(0x43790,CG_LimboPanel_GetWeaponForNumber,forNumber);HOOK(0x43d20,CG_LimboPanel_RealWeaponIsDisabled,isDisabled);
 HOOK(0x43560,CG_LimboPanel_GetPlayerClass,playerClass);HOOK(0x88380,BG_PanelButtons_GetFocusButton,getFocus);HOOK(0x42020,CG_DrawBorder,border);HOOK(0x41c50,CG_LimboPanel_WeaponPanel_DrawWeapon,card);HOOK(0x65070,trap_S_StartLocalSound,sound);
 cgs.media.sndLimboFocus=*(int *)(base+0x2588ddc)=103;cgs.media.limboWeaponBlendThingy=*(int *)(base+0x259d138)=104;cgs.media.limboWeaponCardArrow=*(int *)(base+0x259d150)=105;
 for(team=1;team<=3;++team)for(count=0;count<5;++count)for(old=-1;old<6;++old)for(f=0;f<2;++f)for(hover=0;hover<3;++hover)for(slot=0;slot<3;++slot){
 memset(&button,0,sizeof(button));button.rect=rect;button.data[7]=7;focus=f?&button:NULL;selected=old<0?0:old;cls=old+1;disabled=old&1;cg.clientNum=*(int *)(base+0x40352a4)=0;
 cgs.clientinfo[0].tceScorePlayerClass=*(int *)(base+0x2580bcc)=2;cgs.clientinfo[0].tceClassRating=*(int *)(base+0x2580bc4)=old;cgs.clientinfo[0].tcePreviousClassRating=*(int *)(base+0x2580bc8)=old+2;
 cgs.ccSelectedWeaponNumber=*(int *)(base+0x3fde380)=slot;cgs.ccSelectedWeapon=*(int *)(base+0x3fde388)=old;cgs.ccSelectedWeapon2=*(int *)(base+0x3fde38c)=old;cgs.ccSelectedWeapon3=*(int *)(base+0x3fdf38c)=old;
 events[0]=0;((void(__cdecl *)(void *))(base+0x42150))(&button);strcpy(expected,events);expectedData=button.data[7];a=*(int *)(base+0x3fde388);b=*(int *)(base+0x3fde38c);c=*(int *)(base+0x3fdf38c);button.data[7]=7;events[0]=0;CG_LimboPanel_WeaponPanel(&button);
 if(strcmp(expected,events)||a!=cgs.ccSelectedWeapon||b!=cgs.ccSelectedWeapon2||c!=cgs.ccSelectedWeapon3||expectedData!=button.data[7]){printf("panel mismatch team%d count%d old%d focus%d hover%d slot%d\n%s\n%s\n",team,count,old,f,hover,slot,expected,events);return 1;}++cases;
 }
 printf("%d complete original weapon table/card/panel comparisons passed\n",cases);return 0;
}
