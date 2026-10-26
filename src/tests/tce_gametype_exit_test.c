/* Whole exit-controller regression plus exact timer/bomb/elimination boundaries. */
#define main previous_exit_comparison_main
#include "tce_exit_rules_test.c"
#undef main
static int compare_exit(int scenario,int frame){char expected[8192];int previous=winner,w;
 original=1;events[0]=0;((void(__cdecl*)(void))(b+0x62f10))();strcpy(expected,events);w=winner;
 original=0;winner=previous;events[0]=0;CheckExitRules();
 if(strcmp(expected,events)||w!=winner||level.tceTimelimitHitTime!=*(int*)(b+0x2223920)||level.tceEndRoundTime!=*(int*)(b+0x2223924)||level.teamEliminateTime!=*(int*)(b+0x221a504)||level.lmsWinningTeam!=*(int*)(b+0x221a508)||level.intermissionQueued!=*(int*)(b+0x7ffc5c)){
 fprintf(stderr,"gametype exit case%d frame%d elapsed%d limit%.9g original=%s source=%s\n",scenario,frame,level.timeCurrent-level.startTime,g_timelimit.value,expected,events);return 0;}return 1;}
int main(int argc,char**argv){int result=previous_exit_comparison_main(argc,argv),m,v,k,d,t,f,n=0;int modes[]={2,5,7};int frameTimes[]={0,999,1000,1001,2999,3000,3001,5998,5999,6000};float limits[]={0,.1f,.33333334f,1,279.62027f,279.6203f,35791.39f};
 if(result)return result;
 for(m=0;m<3;m++)for(v=0;v<128;v++)for(k=0;k<7;k++)for(d=-2;d<=2;d++){
  double edge=(double)limits[k]*60000.0;int elapsed=(int)edge+d;
  BOTH(g_gametype.integer,0x6d8d4c,modes[m]);BOTH(g_gamestate.integer,0x784f6c,GS_PLAYING);
  BOTH(level.time,0x7febe0,100000);BOTH(level.startTime,0x7febec,0);BOTH(level.timeCurrent,0x221bc08,elapsed);g_timelimit.value=*(float*)(b+0x7fd628)=limits[k];
  BOTH(level.warmupTime,0x7feb90,0);BOTH(level.intermissionQueued,0x7ffc5c,0);BOTH(level.tceExitRulesNotBefore,0x2223928,0);BOTH(level.tceEndRoundTime,0x2223924,0);BOTH(level.tceTimelimitHitTime,0x2223920,0);
  BOTH(level.tceBombPlanted,0x2223914,v&1);BOTH(g_fraglimit.integer,0x6d89ec,(v&2)?10:0);BOTH(level.teamScores[1],0x7febf4,(v&4)?11:0);BOTH(level.teamScores[2],0x7febf8,(v&8)?11:0);
  BOTH(level.numPlayingClients,0x7fec14,4);BOTH(g_maxlives.integer,0x6d7fcc,1);BOTH(g_axismaxlives.integer,0x23ceecc,0);BOTH(g_alliedmaxlives.integer,0x6d91cc,0);
  BOTH(level.numTeamClients[0],0x7ff240,2);BOTH(level.numTeamClients[1],0x7ff244,2);BOTH(level.numFinalDead[0],0x221a418,(v&16)?2:0);BOTH(level.numFinalDead[1],0x221a41c,(v&32)?2:0);
  BOTH(level.firstbloodTeam,0x221a500,(v>>4)%3);BOTH(level.teamEliminateTime,0x221a504,0);BOTH(level.lmsWinningTeam,0x221a508,0);
  level.gameManager=(v&64)?g_entities+17:NULL;*(void**)(b+0x221b71c)=level.gameManager?b+0x226e2a0+17*0x57c:NULL;winner=(v>>3)%3-1;
  for(f=0;f<10;f++){
   BOTH(level.time,0x7febe0,100000+frameTimes[f]);
   /* First hold the planted state beyond the deadline. Then model a real
    * map script endround producer after defuse/detonation, on both sides. */
   if(f==4&&(v&1)){
    BOTH(level.tceBombPlanted,0x2223914,0);
    ((int(__cdecl*)(void*,void*))(b+0x82e90))(NULL,NULL);G_ScriptAction_EndRound(NULL,NULL);
   }
   if(!compare_exit(n,f))return 1;n++;
  }
 }
 printf("%d additional GT2/5/7 original timer/bomb/elimination frame comparisons passed\n",n);return 0;
}
