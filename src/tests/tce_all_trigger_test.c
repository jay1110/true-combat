/* Reuse capture primitives, not the previous comparison main. */
#define main previous_objective_touch_main
#include "tce_objective_touch_test.c"
#undef main
extern qboolean G_AllInTrigger(gentity_t*);
int main(int argc,char**argv){HMODULE dll;int i,j,x,y,ec,n=0;record expected[32];
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;
 if(!hook(base+0x8afe0,box)||!hook(trap_EntitiesInBox,box))return 2;
 for(i=0;i<65536;i++){
  memset(r,0,sizeof(r));memset(&ent,0,sizeof(ent));memset(sc,0,sizeof(sc));memset(clients,0,sizeof(clients));variant=i>>7;
  ent.spawnflags=*(int*)(r+0x1a8)=i&127;ent.s.teamNum=*(int*)(r+0x108)=(i>>7)&3;
  level.clients=sc;*(void**)(base+0x7feb80)=clients;level.numConnectedClients=*(int*)(base+0x7fec0c)=(i>>9)%6-1;
  for(j=0;j<4;j++){int ix=(j+((i>>11)&3))%4,k;level.sortedClients[j]=*(int*)(base+0x7fec18+4*j)=ix;
   sc[ix].sess.sessionTeam=*(int*)(clients[ix]+0xb30)=(i+j)%4;sc[ix].pers.connected=*(int*)(clients[ix]+0x5ac)=((i>>2)+j)%3;
   sc[ix].ps.stats[STAT_TCE_FLAGS]=*(int*)(clients[ix]+0xf4)=((i>>4)&(1<<j))?0x400:0;
   for(k=0;k<3;k++)sc[ix].ps.origin[k]=*(float*)(clients[ix]+0x14+k*4)=(float)((i%37-j*53)*(.5+k));
  }
  for(j=0;j<3;j++){g_entities[30+j].s.eType=*(int*)(base+0x226e2a0+(30+j)*0x57c+4)=((i>>(j+6))&1)?63:65;g_entities[30+j].s.teamNum=*(int*)(base+0x226e2a0+(30+j)*0x57c+0x108)=(i+j)%4;}
  side=0;count=bad=boxes=0;x=((int(__cdecl*)(void*))(base+0x756d0))(r);ec=count;memcpy(expected,logs,sizeof(expected));
  side=1;count=boxes=0;y=G_AllInTrigger(&ent);
  if(bad||x!=y||ec!=count||memcmp(expected,logs,count*sizeof(record))){fprintf(stderr,"AllInTrigger case%d result%d/%d calls%d/%d failed\n",i,y,x,count,ec);return 1;}n++;
 }
 printf("%d whole G_AllInTrigger original comparisons passed\n",n);return 0;
}
