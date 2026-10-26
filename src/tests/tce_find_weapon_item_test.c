#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <limits.h>
static int errors,code,tag;
static char message[128];
static void hook(unsigned char *p,void *fn) {DWORD old,tmp;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static void err(int level,const char *fmt,...) {va_list ap;++errors;code=level;strncpy(message,fmt,127);message[127]=0;va_start(ap,fmt);tag=va_arg(ap,int);va_end(ap);}
int main(int argc,char **argv) {
 HMODULE dll;unsigned char *base;gitem_t *originalItem,*sourceItem;int n,w,expectedErrors,expectedCode,expectedTag,count=0,module;
 const int lookup[]={0x2b0b0,0x3770},table[]={0xb6b38,0x98f58},error[]={0x61860,0x47c30};char expected[128];
 if(argc<2 || argc>3)return 2;
 for(module=0;module<argc-1;++module) {
 if(!(dll=LoadLibraryA(argv[module+1])))return 2;base=(unsigned char *)dll;
 /* The real production catalog must match, including strings and nullness.
  * Merely making lookup return an alternate pointer would break ITEM_INDEX. */
 {
  int row,field;unsigned int *a,*b;
  if(bg_numItems!=93)return 5;
  for(row=0;row<94;++row) {
   a=(unsigned int *)(base+table[module]+60*row);b=(unsigned int *)&bg_itemlist[row];
   for(field=0;field<15;++field) {
    if(field<8 || field>12) {
     if(!!a[field]!=!!b[field] || (a[field] && strcmp((char *)a[field],(char *)b[field]))) {
      printf("Catalog mismatch row%d stringfield%d\n",row,field);return 6;
     }
    } else if(a[field]!=b[field]) {printf("Catalog mismatch row%d integerfield%d\n",row,field);return 7;}
   }
  }
 }
 hook(base+error[module],err);hook((unsigned char *)Com_Error,err);
 for(n=0;n<515;++n) {
 w=n<513?n-256:n==513?INT_MIN:INT_MAX;
 errors=code=tag=0;message[0]=0;originalItem=((gitem_t *(__cdecl *)(int))(base+lookup[module]))(w);
 expectedErrors=errors;expectedCode=code;expectedTag=tag;strcpy(expected,message);
 errors=code=tag=0;message[0]=0;sourceItem=BG_FindItemForWeapon(w);
 if(!!originalItem!=!!sourceItem || errors!=expectedErrors || code!=expectedCode || tag!=expectedTag || strcmp(message,expected) ||
 (sourceItem && (sourceItem->giTag!=originalItem->giTag || sourceItem->giType!=originalItem->giType || strcmp(sourceItem->classname,originalItem->classname)))) {
 printf("FindItem mismatch id%d original%s source%s errors%d/%d codes%d/%d tags%d/%d\n",w,originalItem?originalItem->classname:"NULL",sourceItem?sourceItem->classname:"NULL",expectedErrors,errors,expectedCode,code,expectedTag,tag);return 1;
 }
 ++count;
 }
 FreeLibrary(dll);
 }
 printf("PASS: %d production item lookup/error comparisons plus1410 catalog fields per module against%d DLLs\n",count,argc-1);return 0;
}
