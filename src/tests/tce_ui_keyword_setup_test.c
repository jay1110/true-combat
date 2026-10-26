#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_shared.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
int main(int argc,char**argv){HMODULE h;unsigned char*b;int mode,pass,bucket,cases=0;assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;
for(mode=0;mode<2;++mode)for(pass=0;pass<3;++pass){keywordHash_t**original=(keywordHash_t**)(b+(mode?0x28e9a0:0x28f1e0));keywordHash_t**source=mode?menuParseKeywordHash:itemParseKeywordHash;keywordHash_t*ot=(keywordHash_t*)(b+(mode?0x38590:0x381a8));keywordHash_t*st=mode?menuParseKeywords:itemParseKeywords;int i;memset(original,0x35,512*4);memset(source,0x35,sizeof(itemParseKeywordHash));((void(__cdecl*)(void))(b+(mode?0x1fa40:0x1f300)))();if(mode)Menu_SetupKeywordHash();else Item_SetupKeywordHash();for(bucket=0;bucket<512;++bucket){keywordHash_t*a=original[bucket],*c=source[bucket];while(a&&c){if(strcmp(a->keyword,c->keyword)){fprintf(stderr,"keyword mismatch mode%d bucket%d original%s source%s\n",mode,bucket,a->keyword,c->keyword);return 1;}assert(a->func&&c->func);assert(a-ot==c-st);a=a->next;c=c->next;}assert(!a&&!c);++cases;}for(i=0;ot[i].keyword;++i){assert(st[i].keyword&&!strcmp(ot[i].keyword,st[i].keyword));assert(((keywordHash_t*(__cdecl*)(keywordHash_t**,const char*))(b+0x1e0c0))(original,ot[i].keyword)==ot+i);assert(KeywordHash_Find(source,st[i].keyword)==st+i);}assert(!st[i].keyword);}
printf("%d original UI keyword bucket comparisons passed\n",cases);FreeLibrary(h);return 0;}
