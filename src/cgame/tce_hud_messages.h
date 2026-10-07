#ifndef TCE_HUD_MESSAGES_H
#define TCE_HUD_MESSAGES_H
typedef struct {
 int health,pmFlags,localTeam,descriptive,gameType,respawns,penalty;
 int client,localClient,followTeam,redTime,blueTime;
 const char *name;
 int objectiveTime,objectiveLines,objectiveCharWidth;
 const char *objective;
} tce_hud_messages_t;
typedef struct {
 const char *(*translate)(const char *);
 const char *(*binding)(const char *);
 int (*width)(const char *,float,int,void *);
 void (*paint)(float,float,float,float,const float *,const char *,float,int,int,void *);
 void (*small)(int,int,const char *,const float *);
 void (*setColor)(const float *);
 float *(*fade)(int,int);
 int (*viewing)(void);
 int (*reinf)(int);
 void *font;
} tce_hud_messages_api_t;
void TCE_DrawLimboMessage(const tce_hud_messages_t *,const tce_hud_messages_api_t *);
int TCE_DrawFollow(const tce_hud_messages_t *,const tce_hud_messages_api_t *);
void TCE_DrawObjectiveInfo(tce_hud_messages_t *,const tce_hud_messages_api_t *);
#endif
