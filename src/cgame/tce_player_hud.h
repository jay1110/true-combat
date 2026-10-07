#ifndef TCE_PLAYER_HUD_H
#define TCE_PLAYER_HUD_H
typedef void (*tce_hudColor_t)(const float *);
typedef void (*tce_hudPic_t)(float,float,float,float,int);
typedef void (*tce_hudBar_t)(float,float,float,float,float *,float *,const float *,float,int);
void TCE_DrawPlayerHealthMan(const float *rect,const int *damage,const int *shaders,
    tce_hudColor_t color,tce_hudPic_t pic);
void TCE_DrawStaminaBar(const float *rect,int sprintTime,int icon,int frame,
    tce_hudColor_t color,tce_hudPic_t pic,tce_hudBar_t bar);
#endif
