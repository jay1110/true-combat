/* Original Windows callbacks 30029100 and 30029460. */
#include "tce_player_hud.h"
void TCE_DrawPlayerHealthMan(const float *r,const int *damage,const int *shaders,
    tce_hudColor_t color,tce_hudPic_t pic) {
    float black[4]={0,0,0,1}, c[4]; int i,health;
    for(i=0;i<3;++i) {
        color(black);pic(r[0]+1,r[1]+1,r[2],r[3],shaders[i]);color(0);
        health=100-damage[i];c[0]=c[3]=1;c[1]=c[2]=1;
        if(health<26){c[1]=0;c[2]=0;}
        else if(health<51){c[1]=.5f;c[2]=0;}
        else if(health<76){c[1]=.9f;c[2]=0;}
        color(c);pic(r[0],r[1],r[2],r[3],shaders[i]);color(0);
    }
    c[0]=c[1]=c[2]=c[3]=1;
    if(damage[0]>89||damage[1]>89||damage[2]>89){c[0]=.9f;c[1]=c[2]=0;}
    color(c);pic(r[0],r[1],r[2],r[3],shaders[3]);color(0);
}
void TCE_DrawStaminaBar(const float *r,int sprintTime,int icon,int frame,
    tce_hudColor_t color,tce_hudPic_t pic,tce_hudBar_t bar) {
    float bg[4]={0,0,0,.3f},fg[4]={.7f,.7f,.7f,.7f};
    float fraction=(float)sprintTime*.00005f;
    pic(r[0],r[1]+r[3]+4,r[2],r[2],icon);
    bar(r[0],r[1]+r[3]*.1f,r[2],r[3]*.84f,fg,0,bg,fraction,0x55);
    color(0);pic(r[0],r[1]+r[3]*.1f,r[2],r[3]*.84f,frame);
}
