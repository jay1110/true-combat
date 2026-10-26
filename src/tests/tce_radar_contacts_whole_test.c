#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_ents.c"
#include <windows.h>
#include <stdio.h>
int main(int argc,char**argv){
 HMODULE h;byte*b;static byte rawsnap[0x600+64*0x120],rawcent[0xa94];static snapshot_t snap;centity_t cent;
 int k,g,i,j,n=0,group,index[3],ages[]={0,1,749,750,751,1000};float*pos;int*times;void(__cdecl*original)(void*);
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(byte*)h;original=(void*)(b+0x31e40);
 cg.snap=&snap;*(void**)(b+0x40352c4)=rawsnap;
 for(k=0;k<16384;k++){
  memset(&cent,0,sizeof(cent));memset(rawcent,0,sizeof(rawcent));memset(&snap,0,sizeof(snap));memset(rawsnap,0,sizeof(rawsnap));
  cg.time=*(int*)(b+0x407dea4)=1000+k%73;snap.ps.clientNum=*(int*)(rawsnap+0xcc)=k%65;
  group=cent.currentState.eFlags=*(int*)(rawcent+8)=k%4-1;
  index[0]=cent.currentState.groundEntityNum=*(int*)(rawcent+0x94)=k%67;
  index[1]=cent.currentState.otherEntityNum=*(int*)(rawcent+0x8c)=(k/7)%67;
  index[2]=cent.currentState.otherEntityNum2=*(int*)(rawcent+0x90)=(k/17)%67;
  for(j=0;j<3;j++){cent.currentState.origin2[j]=11.25f+j;cent.currentState.origin[j]=-23.5f+j;cent.currentState.pos.trBase[j]=57.125f+j;}
  memcpy(rawcent+0x68,cent.currentState.origin2,12);memcpy(rawcent+0x5c,cent.currentState.origin,12);memcpy(rawcent+0x18,cent.currentState.pos.trBase,12);
  for(g=0;g<2;g++){
   pos=(float*)(b+(g?0x40a4334:0x40a3d34));times=(int*)(b+(g?0x40a4634:0x40a4034));
   for(i=0;i<64;i++){cg.tceRadarTimes[g][i]=times[i]=(k+i)%7?cg.time-ages[(k+i)%6]:0;
    for(j=0;j<3;j++)cg.tceRadarPositions[g][i][j]=pos[i*3+j]=(k+i)%11?(i+1)*.125f+j:0;}
  }
  snap.numEntities=*(int*)(rawsnap+0x5d8)=(k/4)%65;
  for(i=0;i<64;i++){
   cg_entities[i].currentState.eFlags=*(int*)(b+0x488d808+i*0xa94)=((i+k)%13==0)?EF_DEAD:0;
   for(j=0;j<3;j++)cg_entities[i].lerpOrigin[j]=((float*)(b+0x488df10+i*0xa94))[j]=(i+1)*37.25f+j;
   snap.entities[i].number=*(int*)(rawsnap+0x5dc+i*0x120)=(i*17+k)%64;
   snap.entities[i].clientNum=*(int*)(rawsnap+0x688+i*0x120)=(i+k)%3?snap.entities[i].number:63;
   cgs.clientinfo[i].infoValid=*(int*)(b+0x2580a10+i*0x1c0)=(i+k)%2;
  }
  original(rawcent);CG_RadarPositions(&cent);
  for(g=0;g<2;g++)if(memcmp(cg.tceRadarPositions[g],b+(g?0x40a4334:0x40a3d34),64*12)||memcmp(cg.tceRadarTimes[g],b+(g?0x40a4634:0x40a4034),64*4)){
   printf("radar contact mismatch case%d group%d array%d\n",k,group,g);return 1;
  }n++;
 }printf("PASS %d whole radar-contact updates, both position/timestamp arrays exact\n",n);return 0;
}
