#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_sound.c"
#include <windows.h>
#include <stdio.h>
int main(int argc,char**argv){HMODULE dll;byte*b;static byte rs[2][100],rl[2][1100];static soundScript_t ss[2];static soundScriptSound_t sl[2];int(__cdecl*original)(const char*);int c,i,j,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x606a0);
 for(c=0;c<16000;++c){const char*name=c%7==0?NULL:c%7==1?"":c%7==2?"missing":"MaTcH";long h=generateHashValue("match");int expected;memset(hashTable,0,sizeof(hashTable));memset(b+0x140ecc8,0,4096);
  for(i=0;i<2;++i){memset(rs[i],0,100);memset(&ss[i],0,sizeof(ss[i]));memset(rl[i],0x5a,1100);memset(&sl[i],0x5a,sizeof(sl[i]));strcpy((char*)rs[i]+4,i?"match":"other");strcpy(ss[i].name,i?"match":"other");ss[i].index=*(int*)rs[i]=c+i;ss[i].streaming=*(int*)(rs[i]+76)=c%2;ss[i].soundList=c%3?&sl[0]:NULL;*(void**)(rs[i]+92)=c%3?rl[0]:NULL;sl[i].numsounds=*(int*)(rl[i]+1088)=(c+i)%17;sl[i].next=i?NULL:&sl[1];*(void**)(rl[i]+1096)=i?NULL:rl[1];for(j=0;j<16;++j)sl[i].sounds[j].sfxHandle=*(int*)(rl[i]+64+68*j)=c+i+j+1;}
  ss[0].nextHash=&ss[1];*(void**)(rs[0]+96)=rs[1];hashTable[h]=&ss[c%2];*(void**)(b+0x140ecc8+h*4)=rs[c%2];expected=original(name);if(expected!=CG_SoundScriptPrecache(name)){printf("precache return%d\n",c);return 1;}
  for(i=0;i<2;++i)for(j=0;j<16;++j)if(sl[i].sounds[j].sfxHandle!=*(int*)(rl[i]+64+68*j)||memcmp(sl[i].sounds[j].filename,rl[i]+68*j,64)){printf("precache mutation%d/%d/%d\n",c,i,j);return 1;}++n;
 }
 printf("%d whole sound-script precache comparisons passed\n",n);return 0;
}
