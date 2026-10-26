/* Reuse the raw original UI layout fixture, not a second implementation. */
#define main previous_feeder_test_main
#include "tce_ui_feeders_test.c"
#undef main
static int pingCase,loadCase,gameCase,flagsCase,requests,favourites;static float official;
static float cvalue(const char*n){sprintf(trace+strlen(trace),"cvar:%s;",n);return official;}
static void serverinfo(int source,int idx,char*out,int size){char v[1024];static int p[]={-1,0,1,75,999},l[]={-1,0,40,41,75,76},g[]={-1,0,2,5,7,8,16};requests++;sprintf(trace+strlen(trace),"info:%d:%d;",source,idx);sprintf(v,"\\addr\\127.0.0.1:%d\\hostname\\TC %d\\mapname\\obj_%d\\clients\\%d\\sv_maxclients\\16\\ping\\%d\\serverload\\%d\\gametype\\%d\\nettype\\%d\\needpass\\%d\\friendlyFire\\%d\\maxlives\\%d\\punkbuster\\%d\\weaprestrict\\%d\\g_antilag\\%d\\balancedteams\\%d",27960+idx,idx,idx,idx,p[pingCase],l[loadCase],g[gameCase],idx%3,flagsCase&1,flagsCase&2,flagsCase&4,flagsCase&8,flagsCase&16?99:100,flagsCase&32,flagsCase&64);Q_strncpyz(out,v,size);}
static int favorite(int source,int i){sprintf(trace+strlen(trace),"fav:%d:%d;",source,i);return favourites;}
static void textsetup(unsigned char*b,int mask){int i,j;setup(b,mask,-1,4,7);
#define COUNT(member,off) uiInfo.member=*(int*)(b+off)=4
 COUNT(serverStatus.numDisplayServers,0x2d1304);COUNT(serverStatusInfo.numLines,0x2d2870);COUNT(playerCount,0x2b348c);COUNT(myTeamCount,0x2b3490);COUNT(modCount,0x2ce2cc);COUNT(movieCount,0x2ceadc);COUNT(demoCount,0x2ce6d4);COUNT(profileCount,0x2ce0c0);
 uiInfo.numGameTypes=*(int*)(b+0x2b3280)=8;uiInfo.numGlInfoLines=*(int*)(b+0x2d836c)=8;
 strcpy(uiInfo.uiDC.glconfig.vendor_string,"Vendor fixture");strcpy((char*)b+0x2b07e4,"Vendor fixture");strcpy(uiInfo.uiDC.glconfig.version_string,"Version fixture");strcpy((char*)b+0x2b0be4,"Version fixture");strcpy(uiInfo.uiDC.glconfig.renderer_string,"Renderer fixture");strcpy((char*)b+0x2b03e4,"Renderer fixture");
 uiInfo.uiDC.glconfig.colorBits=*(int*)(b+0x2b1fec)=24;uiInfo.uiDC.glconfig.depthBits=*(int*)(b+0x2b1ff0)=24;uiInfo.uiDC.glconfig.stencilBits=*(int*)(b+0x2b1ff4)=8;
 for(i=0;i<8;i++){uiInfo.gameTypes[i].gtEnum=*(int*)(b+0x2b328c+i*16)=i==3?15:i;uiInfo.gameTypes[i].gameTypeShort=*(const char**)(b+0x2b3288+i*16)=names[i%4];uiInfo.glInfoLines[i]=*(const char**)(b+0x2d816c+i*4)=names[i%4];}
 for(i=0;i<4;i++){
  strcpy(uiInfo.foundPlayerServerNames[i],names[i]);strcpy((char*)b+0x2d393c+i*64,names[i]);strcpy(uiInfo.playerNames[i],names[i]);strcpy((char*)b+0x2b34a8+i*72,names[i]);strcpy(uiInfo.teamNames[i],names[i]);strcpy((char*)b+0x2b48a8+i*36,names[i]);
  for(j=0;j<4;j++)uiInfo.serverStatusInfo.lines[i][j]=*(const char**)(b+0x2d17b0+(i*4+j)*4)=names[(i+j)%4];
  uiInfo.modList[i].modName=*(const char**)(b+0x2ce0cc+i*8)=names[i];uiInfo.modList[i].modDescr=*(const char**)(b+0x2ce0d0+i*8)=i==0?NULL:i==1?"":names[(i+1)%4];
  uiInfo.movieList[i]=*(const char**)(b+0x2ce6dc+i*4)=names[i];uiInfo.demoList[i]=*(const char**)(b+0x2ce2d4+i*4)=names[i];
 }
 strcpy(cl_profile.string,mask&1?"first":"second");strcpy((char*)b+0x2dc870,cl_profile.string);strcpy(cl_defaultProfile.string,mask&2?"first":"third");strcpy((char*)b+0x291a90,cl_defaultProfile.string);
 uiInfo.profileList[0].name=*(const char**)(b+0x2cdec0)="^1first";
 uiInfo.passwordFilter=*(int*)(b+0x2d8390)=11;uiInfo.friendlyFireFilter=*(int*)(b+0x2d8394)=12;uiInfo.maxLivesFilter=*(int*)(b+0x2d8398)=13;uiInfo.punkBusterFilter=*(int*)(b+0x2d839c)=14;uiInfo.weaponRestrictionsFilter=*(int*)(b+0x2d83a0)=15;uiInfo.antiLagFilter=*(int*)(b+0x2d83a4)=16;uiInfo.teamBalanceFilter=*(int*)(b+0x2d83a8)=17;
 uiInfo.uiDC.Assets.checkboxCheck=*(int*)(b+0x2b032c)=21;uiInfo.uiDC.Assets.checkboxCheckNot=*(int*)(b+0x2b0330)=22;
}
static int checktext(unsigned char*b,float f,int idx,int col,int serial){char saved[2048],savedtrace[8192];int eh[8],ah[8],en=-99,an=-99,i;const char *r;
 for(i=0;i<8;i++)eh[i]=ah[i]=0x5555;
 trace[0]=0;r=((const char*(__cdecl*)(float,int,int,int*,int*))(b+0xc650))(f,idx,col,eh,&en);Q_strncpyz(saved,r,sizeof(saved));strcpy(savedtrace,trace);trace[0]=0;r=UI_FeederItemText(f,idx,col,ah,&an);
 if(strcmp(saved,r)||strcmp(savedtrace,trace)||en!=an||memcmp(eh,ah,sizeof(eh))){printf("text mismatch %d f%.1f idx%d col%d original:%s source:%s handles%d/%d trace%s/%s\n",serial,f,idx,col,saved,r,en,an,savedtrace,trace);return 0;}return 1;
}
int main(int argc,char**argv){HMODULE h;unsigned char*b;int f,i,j,k,mask,mode,sp,cases=0,n,c,expected,got;char saved[8192];const int modes[]={0,2,4,5,7,31},offs[]={-1,0,1,2};
 const float feeders[]={FEEDER_HEADS,FEEDER_Q3HEADS,FEEDER_MAPS,FEEDER_ALLMAPS,FEEDER_CAMPAIGNS,FEEDER_ALLCAMPAIGNS,FEEDER_SAVEGAMES,FEEDER_GLINFO,FEEDER_SERVERSTATUS,FEEDER_FINDPLAYER,FEEDER_PLAYER_LIST,FEEDER_TEAM_LIST,FEEDER_MODS,FEEDER_CINEMATICS,FEEDER_DEMOS,FEEDER_PROFILES,99};
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char*)h;
 PATCH(0x20ba0,serverinfo,trap_LAN_GetServerInfo);PATCH(0x20c60,favorite,trap_LAN_ServerIsInFavoriteList);PATCH(0x20550,cvalue,trap_Cvar_VariableValue);
 for(mask=0;mask<16;mask++)for(f=0;f<17;f++)for(i=-1;i<10;i++)for(j=-1;j<5;j++){textsetup(b,mask);if(!checktext(b,feeders[f],i,j,cases++))return 3;}
 textsetup(b,15);
 for(pingCase=0;pingCase<5;pingCase++)for(loadCase=0;loadCase<6;loadCase++)for(gameCase=0;gameCase<7;gameCase++)for(flagsCase=0;flagsCase<128;flagsCase++){
  ui_netSource.integer=*(int*)(b+0x28fd4c)=flagsCase%3;favourites=flagsCase%2;
  uiInfo.uiDC.realTime=*(int*)(b+0x292140)=flagsCase&1?10000:0;
  for(j=-1;j<8;j++){if(!checktext(b,FEEDER_SERVERS,flagsCase%4,j,cases++))return 4;}
  /* Repeated column and changed row exercises original cache reuse. */
  if(!checktext(b,FEEDER_SERVERS,(flagsCase+1)%4,7,cases++))return 5;
 }
 for(mode=0;mode<6;mode++)for(sp=0;sp<2;sp++)for(mask=0;mask<16;mask++)for(c=0;c<5;c++)for(n=0;n<4;n++){
  setup(b,mask,-1,c,modes[mode]);official=offs[n];
  for(i=0;i<4;i++){uiInfo.mapList[i].typeBits=*(int*)(b+0x2b52b8+i*0x88)=(i&1)?0xffffffff:1u<<modes[(mode+1)%6];uiInfo.mapList[i].tceOfficial=*(int*)(b+0x2b5330+i*0x88)=(mask>>i)&1;}
  trace[0]=0;expected=((int(__cdecl*)(int))(b+0xb360))(sp);strcpy(saved,trace);trace[0]=0;got=UI_MapCountByGameType(sp);
  if(expected!=got||strcmp(saved,trace)){printf("mapcount mismatch mode%d sp%d mask%d count%d official%.1f\n",modes[mode],sp,mask,c,official);return 6;}for(i=0;i<4;i++)if(uiInfo.mapList[i].active!=*(int*)(b+0x2b530c+i*0x88))return 7;cases++;
 }
 printf("%d whole text-feeder/map-count comparisons passed\n",cases);return 0;
}
