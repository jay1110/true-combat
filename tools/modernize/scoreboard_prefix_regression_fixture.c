/* Actual scoreboard rows and name helpers; renderer/configstring services are controlled. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "src/cgame/tce_client_score.h"
#include "src/game/tce_countryflags.h"

#define MAX_CLIENTS 64
#define MAX_NAME_LENGTH 64
#define TEAM_SPECTATOR 3
#define CS_PLAYERS 100
#define PERS_TEAM 0
#define STAT_HEALTH 0
typedef float vec4_t[4];
typedef tce_score_t score_t;
typedef struct { char name[MAX_NAME_LENGTH]; int team, powerups, health; } clientInfo_t;
typedef struct { int clientNum, persistant[1], stats[1], pm_type; } playerState_t;
typedef struct { playerState_t ps; } snapshot_t;
static struct { score_t scores[MAX_CLIENTS]; snapshot_t *snap; int clientNum; } cg;
static struct {
    clientInfo_t clientinfo[MAX_CLIENTS];
    struct { int limboFont1, objectiveShader, scoreEliminatedShader, medicIcon; } media;
    int gametype, tceKillMessage;
} cgs;
static struct { int integer; } cg_countryflags, cg_gameType;
static int tce_scoreExtended[MAX_CLIENTS], tce_countryFlagsShader;
static const char *country_value;
static int shader_available, config_calls, registrations;
static int checks, failures, cases, route_failures[3], active_route;
static const char *case_label;
typedef struct { float x, y, width, height, s, t, s2, t2; int shader; char text[128]; } event_t;
static event_t texts[24], flags[4];
static int text_count, flag_count, pic_count, color_set, color_clear;
static float active_color[4];

static void expect(int condition, const char *message)
{
    ++checks;
    if (!condition) {
        if (failures < 24) fprintf(stderr, "route=%d case=%d %s: %s\n", active_route, cases, case_label, message);
        ++failures; ++route_failures[active_route];
    }
}
static int near(float a, float b) { return fabsf(a-b) < .00001f; }
static void Q_strncpyz(char *out, const char *in, size_t count)
{ if (count) { size_t n=strlen(in); if(n>=count)n=count-1; memcpy(out,in,n);out[n]=0; } }
static const char *CG_ConfigString(int index)
{ expect(index==CS_PLAYERS+1,"configstring client identity"); ++config_calls; return country_value; }
static const char *Info_ValueForKey(const char *value, const char *key)
{ expect(strcmp(key,"u")==0,"country key"); return value; }
static int trap_R_RegisterShaderNoMip(const char *name)
{ expect(strcmp(name,"gfx/flags/world_flags")==0,"atlas shader name"); ++registrations; return shader_available ? 77 : 0; }
static void trap_R_SetColor(const float *color)
{ if(color) {memcpy(active_color,color,sizeof(active_color));++color_set;} else ++color_clear; }
static void CG_DrawPicST(float x,float y,float w,float h,float s,float t,float s2,float t2,int shader)
{
    event_t *e;
    expect(flag_count<4,"flag callback capacity"); if(flag_count>=4)return;
    e=&flags[flag_count++];e->x=x;e->y=y;e->width=w;e->height=h;
    e->s=s;e->t=t;e->s2=s2;e->t2=t2;e->shader=shader;
    expect(near(active_color[0],1)&&near(active_color[1],1)&&near(active_color[2],1)&&near(active_color[3],.6f),"flag fade color");
}
static int CG_Text_Width_Ext(const char *text,float scale,int limit,void *font)
{ (void)scale;(void)limit;(void)font; return (int)strlen(text)*6; }
static void CG_Text_Paint_Ext(float x,float y,float sx,float sy,float *color,const char *text,int a,int b,int style,void *font)
{
    event_t *e;(void)a;(void)b;(void)style;(void)font;
    expect(text_count<24,"text callback capacity");if(text_count>=24)return;
    expect(near(sx,.2f)&&near(sy,.2f),"text scale"); expect(near(color[3],.6f),"text fade");
    e=&texts[text_count++];e->x=x;e->y=y;Q_strncpyz(e->text,text,sizeof(e->text));
}
static void CG_FillRect(float x,float y,float w,float h,const float *color)
{ (void)x;(void)y;(void)w;(void)h;(void)color; }
static void CG_DrawPic(float x,float y,float w,float h,int shader)
{ (void)x;(void)y;(void)w;(void)h;(void)shader;++pic_count; }
static const char *CG_TranslateString(const char *text) { return text; }
static int CG_DrawStrlen(const char *text) { return (int)strlen(text); }
static int CG_mvMergedClientLocate(int client) { return client; }
static char *va(const char *format,...)
{ static char ring[16][128];static unsigned index;char *text=ring[(index++)&15];va_list args;va_start(args,format);vsnprintf(text,128,format,args);va_end(args);return text; }

/* ACTUAL_HELPERS */
/* ACTUAL_WRAPPERS */

typedef struct { const char *value; int atlas; } country_case_t;
static const country_case_t countries[]={
    {"38",38},{"0",1},{"254",1},{"255",1},{"",-1},{"bad",-1},
    {"-1",-1},{"256",-1},{"38junk",-1},{"38 ",-1},{"0x26",-1},
    {"9999999999999999999999999999999999999",-1}
};
static const char *names[]={"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789","ABCDEFGHIJKLMNOPQR^tail","A"};
static const char *clipped[2][3]={
    {"ABCDEFGHIJKLMNOPQRSTUV","ABCDEFGHIJKLMNOPQR^tai","A"},
    {"ABCDEFGHIJKLMNOPQRS","ABCDEFGHIJKLMNOPQR","A"}
};
static void row_case(int route,int ping,int enabled,int country,int name,int shader,int y)
{
    snapshot_t snap;float color[4]={1,1,1,.6f};int prefix,visible,expected_texts,i;char label[128];
    memset(&cg,0,sizeof(cg));memset(&cgs,0,sizeof(cgs));memset(&snap,0,sizeof(snap));
    memset(texts,0,sizeof(texts));memset(flags,0,sizeof(flags));
    active_route=route;snprintf(label,sizeof(label),"ping=%d enabled=%d country=[%s] name=%d shader=%d y=%d",ping,enabled,countries[country].value,name,shader,y);case_label=label;
    cg.snap=&snap;cg.clientNum=0;snap.ps.clientNum=0;snap.ps.persistant[PERS_TEAM]=2;snap.ps.stats[STAT_HEALTH]=100;
    cgs.clientinfo[0].team=2;cgs.clientinfo[1].team=1;cgs.clientinfo[1].health=100;
    Q_strncpyz(cgs.clientinfo[1].name,names[name],sizeof(cgs.clientinfo[1].name));
    cgs.gametype=5;cgs.tceKillMessage=1;cg_gameType.integer=5;
    cg.scores[0].client=1;cg.scores[0].ping=ping;cg.scores[0].score=13;cg.scores[0].kills=7;
    cg.scores[0].deaths=8;cg.scores[0].suicides=2;cg.scores[0].teamKills=1;cg.scores[0].damageRating=1234;cg.scores[0].classRating=4;
    tce_scoreExtended[0]=route==2;cg_countryflags.integer=enabled;country_value=countries[country].value;
    shader_available=shader;tce_countryFlagsShader=0;config_calls=registrations=0;
    text_count=flag_count=pic_count=color_set=color_clear=0;
    if(route==0)TCE_ScoreLegacyClientRow(20,y,0,color,.6f);else TCE_ScoreClientRow(20,y,0,color,.6f);
    visible=y<=453;prefix=visible&&enabled&&shader&&countries[country].atlas>=0&&ping!=-1&&ping!=999;
    expected_texts=route==2?9:9;
    expect(flag_count==prefix,"prefix drawing callback count");
    expect(text_count==(visible?expected_texts:0),"row text callback count");
    expect(pic_count==0,"ordinary row has no objective/health icons");
    expect(color_set==prefix&&color_clear==prefix,"prefix color pair");
    if(visible&&text_count) {
        expect(near(texts[0].x,prefix?38.f:20.f)&&near(texts[0].y,(float)y),"name origin reserves exactly prefix width");
        expect(strcmp(texts[0].text,clipped[prefix][name])==0,"actual name callback clips text and trailing caret");
        expect(near(texts[1].x,160.f),"first numeric column remains fixed");
        expect(near(texts[8].x,382.f),"ping column remains fixed");
        for(i=1;i<text_count;++i)expect(near(texts[i].y,(float)y),"numeric callback row position");
    }
    if(prefix&&flag_count) {
        int atlas=countries[country].atlas;
        expect(near(flags[0].x,20)&&near(flags[0].y,(float)y-12)&&near(flags[0].width,14)&&near(flags[0].height,11),"flag rectangle");
        expect(flags[0].shader==77,"actual registered shader reaches draw callback");
        expect(near(flags[0].s,(float)(atlas&15)/16)&&near(flags[0].t,(float)(atlas>>4)/16)&&near(flags[0].s2,flags[0].s+1.f/16)&&near(flags[0].t2,flags[0].t+1.f/16),"country atlas coordinates");
    }
    if(visible&&(ping==-1||ping==999))expect(config_calls==0&&registrations==0,"connecting rows avoid country/config/shader side effects");
    if(y==80&&enabled&&shader&&name==0&&(country==0||country==5)&&(ping==42||ping==-1||ping==999))
        printf("TRACE route=%d ping=%d country=[%s] prefixes=%d name_x=%.0f name=[%s] first_column=%.0f ping_column=%.0f\n",route,ping,countries[country].value,flag_count,text_count?texts[0].x:-1.f,text_count?texts[0].text:"",text_count>1?texts[1].x:-1.f,text_count>8?texts[8].x:-1.f);
    ++cases;
}
int main(void)
{
    int route,ping,enabled,country,name;const int pings[]={-1,999,0,42,1000};
    for(route=0;route<3;++route)for(ping=0;ping<5;++ping)for(enabled=0;enabled<2;++enabled)
    for(country=0;country<(int)(sizeof(countries)/sizeof(countries[0]));++country)for(name=0;name<3;++name)
        row_case(route,pings[ping],enabled,country,name,1,80);
    for(route=0;route<3;++route){row_case(route,42,1,0,0,0,80);row_case(route,42,1,0,0,1,453);row_case(route,42,1,0,0,1,454);}
    printf("scoreboard prefix cases=%d checks=%d failures=%d route_failures=%d,%d,%d\n",cases,checks,failures,route_failures[0],route_failures[1],route_failures[2]);
    return failures?1:0;
}
