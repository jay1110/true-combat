/* Full production movement functions; same shared implementation is linked in both modules. */
#include "../game/bg_pmove.c"
#include <windows.h>
#include <stdio.h>
static int events,ev,bodies,anim,cont,force,conditions,condValue,condClient,condType,condConvert;
static void patch(void *address,void *fn){unsigned char *p=address;DWORD old,tmp;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static void originalEvent(int e){++events;ev=e;}
static void sourceEvent(int e,int parm,playerState_t *ps){originalEvent(e);}
static int body(void *ps,void *model,int a,int c,int f){++bodies;anim=a;cont=c;force=f;return 0;}
static void condition(int client,int type,int value,int convert){++conditions;condClient=client;condType=type;condValue=value;condConvert=convert;}
static void reset(void){events=bodies=conditions=ev=anim=cont=force=condValue=condClient=condType=condConvert=0;}
static int bodyMap(int a){switch(a){case 7:return ANIM_ET_DROPWEAPON;case 8:return ANIM_ET_RAISEWEAPON;case 26:return ANIM_ET_DO_ALT_WEAPON_MODE;case 27:return ANIM_ET_UNDO_ALT_WEAPON_MODE;case 28:return ANIM_ET_DO_ALT_WEAPON_MODE_PRONE;case 29:return ANIM_ET_UNDO_ALT_WEAPON_MODE_PRONE;case 32:return ANIM_ET_RAISEWEAPONPRONE;default:return a;}}
int main(int argc,char **argv){
 static playerState_t ps;static pmove_t move;static pmoveExt_t ext;static bg_character_t character;
 unsigned char raw[0x700],rawpm[0x200],rawext[0x200],rawchar[0x900],*b;HMODULE dll;
 int module,gear,phase,old,next,s,i,count=0;int ex[11];vec3_t angles={17,38,-11};
 unsigned pmOffset,pmlOffset,altsOffset,defsOffset,beginOffset,finishOffset;int skills[8]={0};
 if(argc!=3)return 2;
 patch(BG_AnimScriptEvent,body);patch(BG_UpdateConditionValue,condition);patch(BG_AddPredictableEventToPlayerstate,sourceEvent);
 for(module=0;module<2;++module){
  if(!(dll=LoadLibraryA(argv[module+1])))return 2;b=(unsigned char *)dll;
  pmOffset=module?0x23cf4fc:0x4ba9edc;pmlOffset=module?0x23cf460:0x4ba9e40;
  altsOffset=module?0x144908:0x106e78;defsOffset=module?0x24435c0:0x4c1df20;
  beginOffset=module?0x31200:0x8e90;finishOffset=module?0x3af10:0x12bd0;
  patch(b+(module?0x2a000:0x2520),body);patch(b+(module?0x2a0a0:0x25f0),condition);patch(b+(module?0x309f0:0x8680),originalEvent);
  for(gear=0;gear<2;++gear){
   gearDef.parsed=gear;
   for(i=-32;i<256;++i){
    if(PM_RaiseAnimForWeapon(i)!=((int (__cdecl *)(int))(b+(module?0x30c30:0x88c0)))(i) ||
       PM_DropAnimForWeapon(i)!=((int (__cdecl *)(int))(b+(module?0x30ca0:0x8930)))(i) ||
       PM_AltSwitchFromForWeapon(i)!=((int (__cdecl *)(int))(b+(module?0x30a80:0x8710)))(i) ||
       PM_AltSwitchToForWeapon(i)!=((int (__cdecl *)(int))(b+(module?0x30a90:0x8720)))(i))return 3;
   }
   for(phase=0;phase<2;++phase)for(old=0;old<64;++old)for(next=-1;next<66;++next)for(s=0;s<16;++s){
    memset(&ps,0,sizeof(ps));memset(&move,0,sizeof(move));memset(&ext,0,sizeof(ext));memset(&character,0,sizeof(character));
    memset(raw,0,sizeof(raw));memset(rawpm,0,sizeof(rawpm));memset(rawext,0,sizeof(rawext));memset(rawchar,0,sizeof(rawchar));
    pm=&move;move.ps=&ps;move.pmext=&ext;move.character=&character;move.skill=skills;
    *(void **)(b+pmOffset)=rawpm;*(void **)rawpm=raw;*(void **)(rawpm+4)=rawext;*(void **)(rawpm+8)=rawchar;*(void **)(rawpm+0x118)=skills;
    ps.weapon=*(int *)(raw+0xa4)=old;ps.nextWeapon=*(int *)(raw+0x41c)=phase?next:12;
    ps.weaponstate=*(int *)(raw+0xa8)=phase?(s&1?4:3):s==2?3:s==3?4:0;
    ps.pm_flags=*(int *)(raw+0xc)=s==1?PMF_RESPAWNED:0;
    ps.weaponDelay=*(int *)(raw+0x30)=s==4?1:0;ps.grenadeTimeLeft=*(int *)(raw+0x34)=s==5?10:-1;
    ps.eFlags=*(int *)(raw+0x68)=s==6 || s==12?EF_PRONE:0;move.waterlevel=*(int *)(rawpm+0x110)=s==7?3:0;
    ps.weaponTime=*(int *)(raw+0x2c)=19;ps.weapAnim=*(int *)(raw+0x480)=s&1?130:2;
    ps.clientNum=*(int *)(raw+0xa0)=17;ext.silencedSideArm=*(int *)(rawext+0xc)=7;
    ps.aimSpreadScale=*(int *)(raw+0x48c)=13;ps.aimSpreadScaleFloat=*(float *)(raw+0x488)=13;
    memset(weapAlts,0,sizeof(weapAlts));memset(b+altsOffset,0,256);
    weapAlts[old]=((int *)(b+altsOffset))[old]=s>=9?next:0;
    if(next>=0 && next<64 && s!=8){COM_BitSet(ps.weapons,next);((unsigned *)(raw+0x3d4))[next/32]|=1u<<(next%32);}
    for(i=0;i<64;++i){ps.ammo[i]=s==15?0:3;ps.ammoclip[i]=s>=13?0:1;memset(&weaponDef[i],0,sizeof(weaponDef[i]));weaponDef[i].maxclip=30;weaponDef[i].singleReload=s==14;}
    memcpy(raw+0x190,ps.ammo,256);memcpy(raw+0x290,ps.ammoclip,256);memcpy(b+defsOffset,weaponDef,sizeof(weaponDef));
    AngleVectors(angles,pml.forward,pml.right,pml.up);memcpy(b+pmlOffset,pml.forward,12);memcpy(b+pmlOffset+12,pml.right,12);
    reset();if(phase)((void (__cdecl *)(void))(b+finishOffset))();else ((void (__cdecl *)(int,int,int))(b+beginOffset))(old,next,s==10);
    ex[0]=events;ex[1]=ev;ex[2]=bodies;ex[3]=bodyMap(anim);ex[4]=cont;ex[5]=force;ex[6]=conditions;ex[7]=condValue;ex[8]=condClient;ex[9]=condType;ex[10]=condConvert;
    reset();if(phase)PM_FinishWeaponChange();else PM_BeginWeaponChange(old,next,s==10);
    if(ps.weapon!=*(int *)(raw+0xa4) || ps.nextWeapon!=*(int *)(raw+0x41c) || ps.weaponstate!=*(int *)(raw+0xa8) ||
       ps.weaponTime!=*(int *)(raw+0x2c) || ps.weapAnim!=*(int *)(raw+0x480) || ps.grenadeTimeLeft!=*(int *)(raw+0x34) ||
       ext.silencedSideArm!=*(int *)(rawext+0xc) || ps.aimSpreadScale!=*(int *)(raw+0x48c) || ps.aimSpreadScaleFloat!=*(float *)(raw+0x488) ||
       memcmp(ps.ammo,raw+0x190,256) || memcmp(ps.ammoclip,raw+0x290,256) ||
       events!=ex[0] || ev!=ex[1] || bodies!=ex[2] || (bodies && (anim!=ex[3] || cont!=ex[4] || force!=ex[5])) ||
       conditions!=ex[6] || (conditions && (condValue!=ex[7] || condClient!=ex[8] || condType!=ex[9] || condConvert!=ex[10]))){
       printf("switch mismatch module%d gear%d phase%d old%d next%d scenario%d state%d/%d body%d/%d ev%d/%d\n",module,gear,phase,old,next,s,ps.weaponstate,*(int *)(raw+0xa8),anim,ex[3],ev,ex[1]);return 1;}
    for(i=0;i<3;++i)if(fabs(ext.mountedWeaponAngles[i]-((float *)(rawext+0x40))[i])>.0001f){printf("mounted angle mismatch\n");return 4;}
    ++count;
   }
  }
  FreeLibrary(dll);
 }
 printf("%d whole production switch comparisons against client and server; selectors before/after Gear passed\n",count);return 0;
}
