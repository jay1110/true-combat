#include "tce_levelshot_fade.h"

/* CG_DrawLevelshotFade, original Windows entry 300276b0. */
void TCE_DrawLevelshotFade(int hasSnapshot, int time, int startTime, int shader,
    void (*setColor)(const float *),
    void (*drawPic)(float,float,float,float,int),
    void (*fillRect)(float,float,float,float,const float *)) {
    int elapsed;
    float black[4] = {0,0,0,0}, white[4] = {1,1,1,0};
    if(!hasSnapshot) return;
    elapsed = (int)((unsigned int)time - (unsigned int)startTime);
    if(elapsed > 2000) return;
    /* Original x87 multiplies the stored float reciprocal before rounding. */
    black[3] = (float)(1.0 - (double)elapsed * (double)0.0005000000237487257f);
    if(black[3] < 0.f) black[3] = 0.f;
    if(black[3] > 1.f) black[3] = 1.f;
    white[3] = black[3];
    setColor(white);
    drawPic(0,58,854,364,shader);
    setColor(0);
    fillRect(-5,-85,862,143,black);
    fillRect(-5,422,862,143,black);
}
