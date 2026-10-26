#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
int main(int argc,char**argv){HMODULE dll;byte*b,*table;void*(__cdecl*original[2])(const char*);int count,pattern,q,op,j,n=0;const char*names[]={"Alpha","bETA","Gamma","","Duplicate","DUPLICATE"};const char*queries[]={"alpha","BETA","GAMMA","","duplicate","missing",NULL};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;table=b+0xb6b38;original[0]=(void*)(b+0x2b240);original[1]=(void*)(b+0x2b280);if(bg_numItems<33)return 2;
 for(count=0;count<=31;++count)for(pattern=0;pattern<8;++pattern){for(j=0;j<33;++j){const char*cl=j<=count?names[(j+pattern)%6]:NULL;const char*name=(j+pattern)%7==6?NULL:names[(j+pattern)%6];bg_itemlist[j].classname=*(char**)(table+j*60)=(char*)cl;bg_itemlist[j].pickup_name=*(char**)(table+j*60+28)=(char*)name;}
  for(q=0;q<7;++q)for(op=0;op<2;++op){void*r=original[op](queries[q]);gitem_t*s=op?BG_FindItemForClassName(queries[q]):BG_FindItem(queries[q]);int ri=r?(int)(((byte*)r-table)/60):-1,si=s?(int)(s-bg_itemlist):-1;if(ri!=si){printf("lookup mismatch count%d pattern%d query%d op%d index%d/%d\n",count,pattern,q,op,si,ri);return 1;}++n;}
 }
 printf("%d whole item-name lookup comparisons passed\n",n);FreeLibrary(dll);return 0;
}
