#ifndef TCE_WARMUP_H
#define TCE_WARMUP_H
typedef struct {
 int warmup,time,gameState,minClients,demo,team,pmFlags,gameType,round,defender;
 int count;
} tce_warmup_t;
typedef struct {
 const char *(*translate)(const char *);
 const char *(*binding)(const char *);
 int (*width)(const char *,float,int,void *);
 void (*paint)(float,float,float,float,const float *,const char *,float,int,int,void *);
 void (*sound)(int,int);
 void *font;
 int beep;
} tce_warmup_api_t;
void TCE_DrawWarmup(tce_warmup_t *,const tce_warmup_api_t *);
#endif
