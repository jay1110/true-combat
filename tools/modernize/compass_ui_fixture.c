/* Controlled draw/filesystem services for actual compass and command handlers.
 * This does not simulate the renderer, network transport or real map assets. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include "src/cgame/tce_compass_map.h"

typedef int qboolean;
typedef int qhandle_t;
typedef int fileHandle_t;
typedef float vec3_t[3];
typedef float vec4_t[4];
#define qtrue 1
#define qfalse 0
#define MAX_QPATH 64
#define FS_READ 0
#define YAW 1
#define PERS_TEAM 0
#define ET_EXPLOSIVE_INDICATOR 13
#define ET_ITEM 2
#define TEAM_AXIS 1
#define TEAM_ALLIES 2
#define MAX_CLIENTS 64
#define STAT_HEALTH 0
#define EF_DEAD 1
#define EF_NODRAW 2
#define IT_TEAM 8
#define PW_REDFLAG 6
#define PW_BLUEFLAG 7
#define CC_2D_W 352
#define CC_2D_H 352
typedef struct { int integer; } vmCvar_t;
typedef struct { int team; } clientInfo_t;
typedef struct { int icon, arrow; } bg_playerclass_t;
typedef struct { int eType, onFireStart, teamNum, otherEntityNum2, otherEntityNum, effect3Time, eFlags, modelindex, density; } entityState_t;
typedef struct { entityState_t currentState; float lerpOrigin[3]; int currentValid; } centity_t;
typedef struct { int persistant[4], clientNum, stats[4]; } playerState_t;
typedef struct { int giType, giTag; } gitem_t;
static const gitem_t bg_itemlist[] = { {0,0}, {8,6}, {8,7}, {1,6} };
static const int bg_numItems = 4;
static qhandle_t tceNavigationPlayers[3], tceNavigationObjectives[2], tceNavigationFlags[2];
#define VectorSubtract(a,b,c) do { (c)[0]=(a)[0]-(b)[0]; (c)[1]=(a)[1]-(b)[1]; (c)[2]=(a)[2]-(b)[2]; } while (0)
#define DotProduct(a,b) ((a)[0]*(b)[0]+(a)[1]*(b)[1]+(a)[2]*(b)[2])
typedef struct { playerState_t ps; int numEntities; struct { int number; } entities[16]; } snapshot_t;
typedef struct { float x, y, automapTransformed[2], transformed[2]; } mapEntityData_t;
static mapEntityData_t mapEntities[8];
static int mapEntityCount;
static float automapZoom = 5.159f;
static qboolean tceRadarMapVisible;
static struct {
 int time, mapcoordsValid, nextFrameTeleport, thisFrameTeleport;
 float mapcoordsMins[2], mapcoordsMaxs[2], mapcoordsScale[2];
 struct { float origin[3], viewangles[3]; } predictedPlayerState;
 snapshot_t *snap, *nextSnap;
} cg;
static struct {
 char rawmapname[MAX_QPATH]; int ccLayers, ccSelectedLayer, autoMapOff, autoMapExpanded, autoMapExpandTime;
 float screenXScale, screenYScale;
 int ccLayerCeils[8];
 clientInfo_t clientinfo[64];
 struct { int tceRadarMarkers[2][3][8]; } media;
} cgs;
static centity_t cg_entities[64];
static vmCvar_t cg_drawCompass, cg_gameType;
static const float colorWhite[4] = { 1, 1, 1, 1 };
static int checks, failures, fsCalls, closeCalls, shaderCalls, picCalls, rotationCalls, textCalls, fillCalls, radarCalls;
static float lastX, lastY, lastRotation, lastBackAlpha, lastWidth, lastHeight, lastColor[4];
static int lastPic, lastArrow;
static int drawnPics[32];
static char lastText[64], allowedPath[128], lastPath[128];
static bg_playerclass_t classes[2] = { {101, 111}, {102, 112} };
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; fprintf(stderr, "line %d: %s\n", __LINE__, #x); } } while (0)
static int near(float a, float b) { return fabsf(a-b) < .0002f; }
static int Q_stricmp(const char *a, const char *b) { return strcmp(a,b); }
static void Q_strncpyz(char *a, const char *b, size_t n) { snprintf(a,n,"%s",b); }
static void Com_sprintf(char *p, size_t n, const char *fmt, ...) { va_list v; va_start(v,fmt); vsnprintf(p,n,fmt,v); va_end(v); }
static void CG_Printf(const char *fmt, ...) { (void)fmt; }
static int trap_FS_FOpenFile(const char *p, fileHandle_t *h, int mode) { (void)mode; ++fsCalls; snprintf(lastPath,sizeof(lastPath),"%s",p); *h=!strcmp(p,allowedPath); return *h?128:-1; }
static void trap_FS_FCloseFile(fileHandle_t f) { (void)f; ++closeCalls; }
static int trap_R_RegisterShaderNoMip(const char *p) {
 static const char *paths[]={"player","player_self","player_dead","objective_a","objective_b","flag_terror","flag_specops"};
 int i;char expected[128];++shaderCalls;
 for(i=0;i<7;i++){snprintf(expected,sizeof(expected),"gfx/tce2/nav/%s.tga",paths[i]);if(!strcmp(p,expected))return 201+i;}
 return 77;
}
static void trap_R_SetColor(const float *c) { if(c)memcpy(lastColor,c,sizeof(lastColor)); }
static float AngleSubtract(float a, float b) { float d=a-b; while(d>180)d-=360; while(d<-180)d+=360; return d; }
static bg_playerclass_t *CG_PlayerClassForClientinfo(clientInfo_t *ci, centity_t *e) { (void)e; return &classes[ci->team==2]; }
static void CG_DrawPic(float x,float y,float w,float h,int p) { drawnPics[picCalls++%32]=p;lastX=x;lastY=y;lastWidth=w;lastHeight=h;lastPic=p; }
static void CG_DrawRotatedPic(float x,float y,float w,float h,int p,float a) { lastX=x;lastY=y;lastWidth=w;lastHeight=h;++rotationCalls;lastArrow=p;lastRotation=a; }
static void CG_DrawRadarIcon(int x,int y,int w,int h,const float *origin,const float *target,int s,int a,int b) { (void)x;(void)y;(void)w;(void)h;(void)origin;(void)target;(void)s;(void)a;(void)b;++radarCalls; }
static void CG_FillRect(float x,float y,float w,float h,const float *c) { (void)x;(void)y;(void)w;(void)h;++fillCalls;lastBackAlpha=c[3]; }
static void CG_DrawRect(float x,float y,float w,float h,float n,const float *c) { (void)x;(void)y;(void)w;(void)h;(void)n;CHECK(c[0]>.9f && c[1]>.5f); }
static void CG_DrawStringExt(float x,float y,const char *t,const float *c,int force,int shadow,int cw,int ch,int max) { (void)x;(void)y;(void)force;(void)shadow;(void)cw;(void)ch;(void)max;++textCalls;snprintf(lastText,sizeof(lastText),"%s",t);CHECK(c[0]==1 && c[1]==1 && c[2]==1); }

/* ACTUAL_FUNCTIONS */

static void reset_draw(void) { picCalls=rotationCalls=textCalls=fillCalls=radarCalls=0; }
int main(void)
{
 int yaw, zoom;
 float point[2], st[2], expected[2], radius, mapWidth, mapHeight;
 float target[2]={800,400};
 snapshot_t snap;
 memset(&snap,0,sizeof(snap)); cg.snap=&snap; cg.mapcoordsValid=1;
 cg.mapcoordsMins[0]=-20000;cg.mapcoordsMins[1]=20000;
 cg.mapcoordsMaxs[0]=20000;cg.mapcoordsMaxs[1]=-20000;
 cg.mapcoordsScale[0]=1.f/40000;cg.mapcoordsScale[1]=-1.f/40000;
 /* Actual expanded viewport stays physically square; server-only contacts
  * must follow each viewport instead of retaining 352-wide parse positions. */
 for (zoom=0;zoom<3;++zoom) {
  const int widths[3]={1280,1024,3440}, heights[3]={720,768,1440};
  cgs.screenXScale=widths[zoom]/640.f;cgs.screenYScale=heights[zoom]/480.f;
  CG_ExpandedMapSize(&mapWidth,&mapHeight);
  CHECK(near(mapWidth*cgs.screenXScale,mapHeight*cgs.screenYScale));
  mapEntityCount=1;mapEntities[0].x=0;mapEntities[0].y=0;
  CG_TransformCommandMapEntities(mapWidth,mapHeight);
  CHECK(near(mapEntities[0].transformed[0],mapWidth*.5f) && near(mapEntities[0].transformed[1],mapHeight*.5f));
  CG_TransformCommandMapEntities(100,200);
  CHECK(near(mapEntities[0].transformed[0],50) && near(mapEntities[0].transformed[1],100));
 }
 mapEntityCount=0;
 CHECK(near(CG_CompassRange(),2000));
 cgs.autoMapOff=1; CG_AutomapZoomIn_f(); CHECK(CG_CompassRange()<2000);
 CG_AutomapZoomOut_f(); CHECK(near(CG_CompassRange(),2000));
 for (zoom=0;zoom<100;zoom++) CG_AutomapZoomIn_f();
 CHECK(near(automapZoom,7.43f));
 for (zoom=0;zoom<100;zoom++) CG_AutomapZoomOut_f();
 CHECK(near(automapZoom,1));
 for (zoom=0;zoom<5;zoom++) {
  radius=1400.f+zoom*1000.f;
  for (yaw=0;yaw<360;yaw+=3) {
   CHECK(TCE_CompassPoint(cg.predictedPlayerState.origin,target,(float)yaw,radius,1,point));
   CHECK(TCE_CompassMapTexCoord(cg.predictedPlayerState.origin,cg.mapcoordsMins,cg.mapcoordsMaxs,(float)yaw,radius,point[0],point[1],st));
   CHECK(TCE_CommandMapPoint(target,cg.mapcoordsMins,cg.mapcoordsMaxs,expected));
   CHECK(near(st[0],expected[0]) && near(st[1],expected[1]));
  }
 }
 target[0]=40000;target[1]=0;
 CHECK(TCE_CompassPoint(cg.predictedPlayerState.origin,target,0,2000,1,point) && near(point[0],0) && near(point[1],-1));
 CHECK(!TCE_CommandMapPoint(target,cg.mapcoordsMins,cg.mapcoordsMaxs,point));
 CHECK(!TCE_CompassPoint(NULL,target,0,2000,1,point));
 CHECK(!TCE_CompassPoint(target,target,0,0,1,point));
 CHECK(!TCE_CompassMapTexCoord(target,cg.mapcoordsMins,cg.mapcoordsMins,0,2000,0,0,point));
 target[0]=1000;
 CHECK(TCE_CompassPoint(cg.predictedPlayerState.origin,target,0,2000,0,point) && near(point[1],-.70710678f));
 cg.time=1000;cgs.autoMapExpandTime=0;
 CG_AutomapExpandDown_f(); CHECK(cgs.autoMapExpanded && cgs.autoMapExpandTime==1000);
 CG_AutomapExpandUp_f(); CHECK(!cgs.autoMapExpanded);
 CG_CommandMap_f(); CHECK(cgs.autoMapExpanded);
 CG_CommandMap_f(); CHECK(!cgs.autoMapExpanded);
 /* The FS cache loads real images only and tries blueprint/layer/overview. */
 snprintf(cgs.rawmapname,sizeof(cgs.rawmapname),"obj_delta");cg_drawCompass.integer=2;
 snprintf(allowedPath,sizeof(allowedPath),"levelshots/obj_delta_cc_blueprint.tga");
 CHECK(CG_CommandMapShader()==77 && closeCalls==1 && shaderCalls==1);
 yaw=fsCalls; CHECK(CG_CommandMapShader()==77 && fsCalls==yaw);
 snprintf(cgs.rawmapname,sizeof(cgs.rawmapname),"obj_other");
 snprintf(allowedPath,sizeof(allowedPath),"levelshots/obj_other_cc.tga");
 CHECK(CG_CommandMapShader()==77 && !strcmp(lastPath,allowedPath));
 /* Radar changes layer with player altitude, with global image fallback. */
 cgs.ccLayers=2;cgs.ccLayerCeils[0]=100;cgs.ccLayerCeils[1]=1000;
 cg_drawCompass.integer=1;snprintf(cgs.rawmapname,sizeof(cgs.rawmapname),"radarlayers");
 cg.predictedPlayerState.origin[2]=50;
 snprintf(allowedPath,sizeof(allowedPath),"levelshots/radarlayers_0_cc.tga");
 CHECK(CG_CompassMapShader()==77 && !strcmp(lastPath,allowedPath));
 yaw=fsCalls;CHECK(CG_CompassMapShader()==77 && fsCalls==yaw);
 cg.predictedPlayerState.origin[2]=200;
 snprintf(allowedPath,sizeof(allowedPath),"levelshots/radarlayers_1_cc.tga");
 CHECK(CG_CompassMapShader()==77 && !strcmp(lastPath,allowedPath));
 cg.predictedPlayerState.origin[2]=50;
 snprintf(allowedPath,sizeof(allowedPath),"levelshots/radarlayers_cc.tga");
 CHECK(CG_CompassMapShader()==77 && !strcmp(lastPath,allowedPath));
 cg_drawCompass.integer=2;
 snprintf(allowedPath,sizeof(allowedPath),"levelshots/radarlayers_0_cc_blueprint.tga");
 CHECK(CG_CompassMapShader()==77 && !strcmp(lastPath,allowedPath));
 cg.predictedPlayerState.origin[2]=200;
 snprintf(allowedPath,sizeof(allowedPath),"levelshots/radarlayers_cc_blueprint.tga");
 CHECK(CG_CompassMapShader()==77 && !strcmp(lastPath,allowedPath));
 cg.predictedPlayerState.origin[2]=50;allowedPath[0]=0;
 CHECK(CG_CompassMapShader()==0);
 cg_drawCompass.integer=3;CHECK(CG_CompassMapShader()==0);
 cg.predictedPlayerState.origin[2]=0;
 snprintf(cgs.rawmapname,sizeof(cgs.rawmapname),"missing");
 CHECK(CG_CommandMapShader()==0);
 cgs.ccLayers=2;cgs.ccSelectedLayer=1;cg_drawCompass.integer=1;
 snprintf(cgs.rawmapname,sizeof(cgs.rawmapname),"layered");
 snprintf(allowedPath,sizeof(allowedPath),"levelshots/layered_1_cc.tga");
 CHECK(CG_CommandMapShader()==77);
 snprintf(cgs.rawmapname,sizeof(cgs.rawmapname),"fallback");
 snprintf(allowedPath,sizeof(allowedPath),"levelshots/fallback_cc.tga");
 CHECK(CG_CommandMapShader()==77 && !strcmp(lastPath,allowedPath));
 /* Exact production custom media registration, compass and player renderer. */
 yaw=shaderCalls;CG_RegisterTCENavigationMedia();CHECK(shaderCalls==yaw+7);
 CHECK(tceNavigationPlayers[0]==201 && tceNavigationPlayers[1]==202 && tceNavigationPlayers[2]==203);
 CHECK(tceNavigationObjectives[0]==204 && tceNavigationObjectives[1]==205);
 CHECK(tceNavigationFlags[0]==206 && tceNavigationFlags[1]==207);
 automapZoom=5.159f;tceRadarMapVisible=1;cgs.clientinfo[1].team=2;target[0]=1000;target[1]=0;
 snap.ps.persistant[PERS_TEAM]=2;snap.ps.stats[STAT_HEALTH]=100;
 reset_draw();CG_DrawCompassContact(50,(const float[3]){1000,0,900},0,1);
 CHECK(picCalls==0 && rotationCalls==1 && lastArrow==201);
 CHECK(near(lastX,787) && near(lastY,30) && near(lastRotation,.625f));
 CHECK(lastColor[1]==1 && lastColor[0]<1);
 reset_draw();CG_DrawCompassContact(50,(const float[3]){0,0,0},90,1);CHECK(rotationCalls==1 && near(lastY,45) && near(lastRotation,.375f));
 reset_draw();CG_DrawCompassContact(50,(const float[3]){0,0,0},0,0);CHECK(rotationCalls==1 && lastArrow==202 && lastColor[0]==1 && lastColor[1]==1);
 cgs.clientinfo[1].team=1;reset_draw();CG_DrawCompassContact(50,(const float[3]){0,0,0},0,1);CHECK(lastColor[0]==1 && lastColor[1]<.5f);
 cg_entities[1].currentValid=1;cg_entities[1].currentState.eFlags=EF_DEAD;
 reset_draw();CG_DrawCompassContact(50,(const float[3]){0,0,0},0,1);CHECK(picCalls==1 && rotationCalls==0 && lastPic==203);
 reset_draw();CG_DrawTCENavigationPlayer(0,0,16,16,1,0,0);CHECK(picCalls==1 && lastPic==203);
 cg_entities[1].currentValid=0;reset_draw();CG_DrawCompassContact(50,(const float[3]){0,0,0},0,1);CHECK(rotationCalls==1);
 snap.ps.stats[STAT_HEALTH]=0;reset_draw();CG_DrawTCENavigationPlayer(0,0,16,16,0,0,0);CHECK(picCalls==1 && lastPic==203);
 snap.ps.stats[STAT_HEALTH]=100;
 reset_draw();CG_DrawCompassContact(50,(const float[3]){0,0,0},0,64);CHECK(!picCalls && !rotationCalls);
 /* The TGA top stays upright under the legacy radial-corner rotation ABI. */
 reset_draw();CG_DrawTCENavigationPlayer(0,0,16,16,0,0,0);CHECK(near(lastRotation,.625f));
 reset_draw();CG_DrawTCENavigationPlayer(0,0,16,16,0,0,(90.f-90.f)/360.f);CHECK(near(lastRotation,.625f));
 reset_draw();CG_DrawTCENavigationPlayer(0,0,16,16,0,0,(90.f-0.f)/360.f);CHECK(near(lastRotation,.875f));
 reset_draw();CG_DrawObjectiveLetter(100,100,1,15);CHECK(picCalls==1 && lastPic==204 && textCalls==0 && fillCalls==0);
 reset_draw();CG_DrawObjectiveLetter(100,100,2,15);CHECK(picCalls==1 && lastPic==205);
 reset_draw();CG_DrawObjectiveLetter(100,100,3,15);CHECK(!strcmp(lastText,"C") && fillCalls==1 && lastBackAlpha>=.9f);
 /* An invalid earlier objective must not hide subsequent valid A/B goals. */
 cg_gameType.integer=5;snap.numEntities=3;snap.ps.persistant[0]=2;
 for(yaw=0;yaw<3;yaw++){snap.entities[yaw].number=yaw;cg_entities[yaw].currentState.eType=13;cg_entities[yaw].currentState.onFireStart=yaw;}
 cg_entities[0].currentState.onFireStart=-1;
 reset_draw();CG_DrawTCECommandMapObjectives(10,10,352,352);CHECK(picCalls==2 && drawnPics[0]==204 && drawnPics[1]==205 && textCalls==0);
 for(zoom=1;zoom<=2;zoom++) {
  cg_drawCompass.integer=zoom;
  reset_draw();CG_DrawTCECompassObjective(50,&cg_entities[1],1,0,0,0);CHECK(picCalls==1 && lastPic==204 && radarCalls==0);
  reset_draw();CG_DrawTCECompassObjective(50,&cg_entities[2],2,0,0,0);CHECK(picCalls==1 && lastPic==205 && radarCalls==0);
 }
 /* Actual broadcast GT2 capture/home TOIs carry the delivered flag team:
  * Delta's red-flag capture TOI sits at the Specops blue home, and vice versa. */
 cg_gameType.integer=2;cg_entities[1].currentState.teamNum=TEAM_AXIS;cg_entities[2].currentState.teamNum=TEAM_ALLIES;
 CHECK(CG_TCEFlagMapShader(&cg_entities[1].currentState)==207 && CG_TCEFlagMapShader(&cg_entities[2].currentState)==206);
 for(zoom=1;zoom<=2;zoom++) {
  cg_drawCompass.integer=zoom;
  reset_draw();CG_DrawTCECommandMapObjectives(10,10,352,352);CHECK(picCalls==2 && drawnPics[0]==207 && drawnPics[1]==206 && textCalls==0);
  reset_draw();CG_DrawTCECompassObjective(50,&cg_entities[1],1,0,0,0);CHECK(picCalls==1 && lastPic==207 && radarCalls==0);
  reset_draw();CG_DrawTCECompassObjective(50,&cg_entities[2],2,0,0,0);CHECK(picCalls==1 && lastPic==206 && radarCalls==0);
 }
 cg_entities[3].currentState.eType=ET_ITEM;cg_entities[3].currentState.modelindex=2;cg_entities[3].currentState.density=1;
 CHECK(CG_TCEFlagMapShader(&cg_entities[3].currentState)==207);
 CHECK(CG_TCEFlagItemHasHomeMarker(&cg_entities[3],&snap));
 snap.numEntities=4;snap.entities[3].number=3;
 reset_draw();CG_DrawTCECommandMapObjectives(10,10,352,352);CHECK(picCalls==2);
 /* LaunchItem's original flag entity number identifies a real drop. Never
  * suppress it merely because it lies beside an existing home TOI. */
 cg_entities[3].currentState.otherEntityNum=64;CHECK(!CG_TCEFlagItemHasHomeMarker(&cg_entities[3],&snap));
 reset_draw();CG_DrawTCECommandMapObjectives(10,10,352,352);CHECK(picCalls==3 && drawnPics[2]==207);
 cg_entities[3].currentState.modelindex=1;CHECK(CG_TCEFlagMapShader(&cg_entities[3].currentState)==206);
 reset_draw();CG_DrawTCECompassObjective(50,&cg_entities[3],-1,0,0,0);CHECK(picCalls==1 && lastPic==206);
 cg_entities[3].currentState.otherEntityNum=0;cg_entities[3].lerpOrigin[0]=129;
 CHECK(!CG_TCEFlagItemHasHomeMarker(&cg_entities[3],&snap));
 cg_entities[3].currentState.eFlags=EF_NODRAW;CHECK(!CG_TCEFlagMapShader(&cg_entities[3].currentState));
 cg_entities[3].currentState.eFlags=0;cg_entities[3].currentState.modelindex=-1;CHECK(!CG_TCEFlagMapShader(&cg_entities[3].currentState));
 cg_entities[3].currentState.modelindex=bg_numItems;CHECK(!CG_TCEFlagMapShader(&cg_entities[3].currentState));
 cg_entities[3].currentState.modelindex=3;CHECK(!CG_TCEFlagMapShader(&cg_entities[3].currentState));
 cg_entities[3].currentState.modelindex=1;cg_entities[3].currentState.density=0;CHECK(!CG_TCEFlagMapShader(&cg_entities[3].currentState));
 cg_gameType.integer=5;CHECK(!CG_TCEFlagMapShader(&cg_entities[1].currentState));
 cg_gameType.integer=7;reset_draw();CG_DrawTCECommandMapObjectives(10,10,352,352);CHECK(picCalls==0 && textCalls==0);
 printf("compass checks=%d failures=%d\n",checks,failures);return failures?1:0;
}
