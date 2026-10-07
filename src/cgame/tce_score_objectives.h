#ifndef TCE_SCORE_OBJECTIVES_H
#define TCE_SCORE_OBJECTIVES_H
typedef struct {
 int pmType,gameState,gameType,time,startTime,intermissionStartTime;
 int viewedTeam,currentRound,wonRounds[2],campaignMap,campaignMaps;
 float timelimit;
 const float *backColor,*barColor,*black;
 const char *(*config)(int);
 char *(*infoValue)(const char *,const char *);
 const char *(*translate)(const char *);
 int (*reinforce)(int);
 int (*width)(const char *,float);
 void (*fill)(float,float,float,float,const float *);
 void (*border)(float,float,float,float,int,const float *);
 void (*text)(float,float,float,const float *,const char *);
} tce_objectivesContext_t;
void TCE_DrawScoreWindow(const float *,const char *,int,int,float,int,const tce_objectivesContext_t *);
int TCE_WM_DrawObjectives(int,int,int,float,const tce_objectivesContext_t *);
#endif
