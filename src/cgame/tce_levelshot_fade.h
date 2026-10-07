#ifndef TCE_LEVELSHOT_FADE_H
#define TCE_LEVELSHOT_FADE_H
void TCE_DrawLevelshotFade(int hasSnapshot, int time, int startTime, int shader,
    void (*setColor)(const float *),
    void (*drawPic)(float,float,float,float,int),
    void (*fillRect)(float,float,float,float,const float *));
#endif
