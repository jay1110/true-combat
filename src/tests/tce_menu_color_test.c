#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_local.h"
#include <windows.h>
#include <stdio.h>
extern menuDef_t Menus[];
extern int menuCount;
extern void Script_SetMenuItemColor(itemDef_t *,qboolean *,char **);
int main(int argc,char **argv){
 HMODULE h;unsigned char *b,*rm;static unsigned char ri[4][0x400];static itemDef_t items[4];
 const char *targets[]={"one","group","two*","g*","*","absent","ONE"};
 const char *fields[]={"backcolor","forecolor","bordercolor","FORECOLOR","unknown"};
 const char *colors[]={".1 .2 .3 .4 ;","-1 2 3 4 ;","0 0 0 0 ;","1 1",""};
 const char *names[]={"one","twoA","twoB","other"},*groups[]={"group","group",NULL,"gOther"};
 int target,field,color,seed,i,count=0;char a[512],c[512],*ap,*cp;qboolean abort1,abort2;
 if(argc!=2 || !(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;rm=b+0x255da0;
 menuCount=*(int *)(b+0x245fb8)=1;
 for(target=0;target<7;++target)for(field=0;field<5;++field)for(color=0;color<5;++color)for(seed=0;seed<3;++seed){
  memset(&Menus[0],0,sizeof(Menus[0]));memset(rm,0,0x718);
  Menus[0].window.name=*(char **)(rm+0x20)="test";Menus[0].itemCount=*(int *)(rm+0xc0)=4;
  for(i=0;i<4;++i){
   memset(&items[i],seed?0x3e:0,sizeof(items[i]));memset(ri[i],seed?0x3e:0,sizeof(ri[i]));
   items[i].window.name=*(const char **)(ri[i]+0x20)=names[i];items[i].window.group=*(const char **)(ri[i]+0x28)=groups[i];
   items[i].window.flags=*(int *)(ri[i]+0x48)=seed==2?0x200:0;
   Menus[0].items[i]=&items[i];*(void **)(rm+0x514+i*4)=ri[i];
  }
  sprintf(a,"test \"%s\" %s %s",targets[target],fields[field],colors[color]);strcpy(c,a);ap=a;cp=c;abort1=abort2=seed==2;
  /* Real parsers, menu lookup and wildcard/group matching on both sides. */
  ((void (__cdecl *)(void *,void *,char **))(b+0x15ae0))(NULL,&abort1,&ap);Script_SetMenuItemColor(NULL,&abort2,&cp);
  if((ap==NULL)!=(cp==NULL) || (ap && cp && ap-a!=cp-c) || abort1!=abort2){printf("parser mismatch %s\n",a);return 1;}
  for(i=0;i<4;++i)if(items[i].window.flags!=*(int *)(ri[i]+0x48) ||
   memcmp(items[i].window.foreColor,ri[i]+0x74,16) || memcmp(items[i].window.backColor,ri[i]+0x84,16) || memcmp(items[i].window.borderColor,ri[i]+0x94,16)){
   printf("color mismatch %s item%d\n",a,i);return 1;}
  ++count;
 }
 /* Preserve the intentional parser repair when an original menu is absent:
  * consume its color tuple, otherwise numeric tokens become UI commands. */
 strcpy(c,"missing group forecolor 0 0 0 1 ;");cp=c;Script_SetMenuItemColor(NULL,&abort2,&cp);
 {const char *t;if(!String_Parse(&cp,&t) || strcmp(t,";"))return 3;}
 printf("%d whole menu-color original comparisons and absent-menu regression passed\n",count);return 0;
}
