"""Exercise production aim timing and independent distant-shot sound variants."""
import argparse
from pathlib import Path
import subprocess
from weapon_regression import function
from compat_validate import msvc_environment

ROOT = Path(__file__).resolve().parents[2]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--baseline', action='store_true')
    parser.add_argument('--baseline-ref', default='b702d12d2f9e2b52c46386320392536415dc6dbf',
                        help='Pre-1.0.5 source revision used with --baseline')
    parser.add_argument('--arch', choices=('x86','x64'), default='x86')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    def source(path):
        return (subprocess.check_output(['git','show',args.baseline_ref+':'+path], cwd=ROOT).decode('utf-8')
                if args.baseline else (ROOT/path).read_text(encoding='utf-8'))
    movement = source('src/game/bg_pmove.c')
    weapons = source('src/cgame/cg_weapons.c')
    media_source = source('src/cgame/tce_weapon_file.c')
    prelude = r'''
#include <stdio.h>
#include <string.h>
#include <math.h>
typedef int qboolean;
typedef struct {int legsAnim,torsoAnim,eFlags;} entityState_t;
#define EF_FORCE_END_FRAME 8
typedef struct {int firstFrame,numFrames,flags;} animation_t;
#define ANIMFL_REVERSED 4
typedef float vec3_t[3];
typedef struct { int serverTime,buttons; } cmd_t;
typedef cmd_t usercmd_t;
typedef struct { int proneTime; } ext_t;
typedef struct { cmd_t cmd; ext_t *pmext; } move_t;
static ext_t ext; static move_t move, *pm=&move;
typedef struct { struct { int number,weapon; struct {vec3_t trBase;} pos; } currentState; } centity_t;
typedef struct {int flashEchoSound[4],flashReverbVolume;} tce_weaponInfo_t;
static centity_t cg_entities[1];
typedef struct {int suppressed,subsonic,scoped,unknown_1c0;float unknown_0f8[4];} tce_weaponDef_t;
static tce_weaponDef_t weaponDef[64];
static struct {float deafness;} tceFlash;
static tce_weaponInfo_t tce_cg_weapons[64];
static int tce_customWeaponIcons[64];
#define Q_stricmp _stricmp
'''
    prelude += r'''
typedef struct {int weapon,clientNum,viewheight,eFlags,pm_flags,pm_type,weaponstate,weaponDelay,stats[32],holdable[16];float leanf;vec3_t origin,viewangles,velocity;} playerState_t;
static struct {struct view_t {vec3_t vieworg;vec3_t viewaxis[3];} *refdef_current;int time,clientNum,renderingThirdPerson;playerState_t predictedPlayerState;struct {float tceWeaponWallAmount,tceWeaponWallProneYaw;} pmext;} cg;
static struct {int tceWeaponWallAvoidance;} cgs;
static struct {int integer;} g_weaponWallAvoidance;
static struct view_t camera;
typedef struct {int startsolid,allsolid;float fraction;struct {vec3_t normal;} plane;} trace_t;
static struct {int integer;} cg_weaponWallAvoidance;
static float wallFraction=1;
static int probeMode,traceCalls;static vec3_t traceStarts[4];
#define PITCH 0
#define YAW 1
#define ROLL 2
#define PM_NORMAL 0
#define PMF_LADDER 4
#define BUTTON_ATTACK 1
#define EF_FIRING 128
#define WEAPON_READY 0
#define WEAPON_FIRING 7
#define WEAPON_FIRINGALT 8
#define STAT_TCE_FLAGS 9
#define MASK_SOLID 1
#define TCE_MAX_WEAPONS 64
#define VectorMA(a,s,b,o) do{int k;for(k=0;k<3;k++)o[k]=a[k]+(s)*b[k];}while(0)
static void CG_Trace(trace_t *t,const float *a,const float *b,const float *c,const float *d,int e,int f){memset(t,0,sizeof(*t));t->fraction=wallFraction;if(probeMode){int i=traceCalls++%4;memcpy(traceStarts[i],a,sizeof(vec3_t));if(probeMode==1)t->fraction=i==1?.2f:1.f;if(probeMode==2){t->fraction=.2f;t->plane.normal[2]=1;}if(probeMode==3){t->fraction=i==0?.5f:1.f;t->plane.normal[0]=-1;}if(probeMode==4||probeMode==5){float yaw=atan2f(d[1]-a[1],d[0]-a[0])*57.29578f;t->fraction=(probeMode==4?yaw>=29.f:yaw<=-19.f)?1.f:.3f;}}}
static void AngleVectors(const vec3_t a,vec3_t f,vec3_t r,vec3_t u){float p=a[0]*.01745329252f,y=a[1]*.01745329252f;if(f){f[0]=cosf(p)*cosf(y);f[1]=cosf(p)*sinf(y);f[2]=-sinf(p);}if(r){r[0]=sinf(y);r[1]=-cosf(y);r[2]=0;}if(u){u[0]=sinf(p)*cosf(y);u[1]=sinf(p)*sinf(y);u[2]=cosf(p);}}
#define VectorCopy(a,b) memcpy(b,a,sizeof(vec3_t))
typedef struct { playerState_t ps; struct {vec3_t tceShotAngles;float tceWeaponWallAmount;} pmext; } gclient_t;
typedef struct { gclient_t *client; struct {int weapon;} s; } gentity_t;
static vec3_t forward,right,up,muzzleTrace,muzzleEffect;
static void CalcMuzzlePointForActivate(gentity_t *e,vec3_t f,vec3_t r,vec3_t u,vec3_t o){}
static void CalcMuzzlePoint(gentity_t *e,int w,vec3_t f,vec3_t r,vec3_t u,vec3_t o){}
#define trap_Trace CG_Trace
#define STAT_TCE_SHOT_SEED 0
#define STAT_TCE_WEAPON_FLAGS 1
#define STAT_TCE_AIM_PHASE 2
#define EF_PRONE 0x80000
#define EF_PRONE_MOVING 0x100000
#define EF_DEAD 1
#define PMF_DUCKED 1
typedef struct {int seed,aiming,prone,ducked,movementSpread,shotSpread,phase;vec3_t shotAngles,muzzle,right,up;} tce_bulletAim_t;

static int chosen,echoCalls,lastEcho;
static int test_rand(void){return chosen;}
#define rand test_rand
#define SND_NOCUT 0
#define CHAN_WEAPON 1
#define TCE_TruncateNativeInteger(x) ((int)(x))
#define TCE_FloatIsNaN(x) isnan(x)
#define VectorSubtract(a,b,o) do{int k;for(k=0;k<3;k++)o[k]=a[k]-b[k];}while(0)
static float TCE_VectorNormalizeValue(vec3_t v){float d=sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);int k;for(k=0;k<3;k++)v[k]/=d;return d;}
static int CG_SoundVolume(vec3_t p,float v,float r,int f){return (int)v;}
static void CG_EliteEffectSound(centity_t *c,int v){}
static void trap_S_StartSoundExVControl(const float *p,int e,int c,int sound,int f,int volume){if(p){++echoCalls;lastEcho=sound;}}
'''
    bodies = '\n'.join(function(movement,n) for n in ('PM_TCEAddWeaponCounter','PM_TCEElapsedWeaponCounter','PM_TCEAimPostureSettled'))
    bodies += '\n'+function(weapons,'CG_TCEFireSound855')
    if not args.baseline:
        bodies += '\n'+'\n'.join(function(source('src/game/tce_weapon_wall_state.h'),n) for n in ('TCE_WeaponWallFirearm','TCE_WeaponWallState','TCE_WeaponWallStatePitch','TCE_WeaponWallProneState','TCE_WeaponWallPosePitch','TCE_WeaponWallProneStowed','TCE_WeaponWallPosturePitch','TCE_WeaponWallProneBlocked','TCE_WeaponWallCancelProneFire'))
        bodies += '\n'+'\n'.join(function(source('src/game/tce_weapon_wall.h'),n) for n in ('TCE_WeaponWallFraction','TCE_WeaponWallProneTarget','TCE_WeaponWallProneApproach','TCE_WeaponWallProtectHead','TCE_WeaponWallPitch','TCE_WeaponWallAimPitch','TCE_WeaponWallApproach'))
    if not args.baseline:
        bodies += '\n'+function(source('src/game/g_weapon.c'),'G_TCEWallShotAngles')
    bodies += '\n'+function(source('src/game/g_weapon.c'),'CalcMuzzlePoints')
    bodies += '\n'+function(source('src/game/tce_bullet.c'),'TCE_BulletEndpos')
    bodies += '\n'+function(source('src/game/g_weapon.c'),'Bullet_Endpos')
    if not args.baseline:
        bodies += '\n'+function(source('src/game/g_client.c'),'G_TCECorpseAnimations')
        bodies += '\n'+function(source('src/cgame/tce_corpse_frame.h'),'TCE_CorpseTerminalFrame')
    else:
        bodies += '\nstatic void TCE_WeaponWallProtectHead(playerState_t*p,const vec3_t a,const vec3_t b,int l,float m,void(*t)(trace_t*,const vec3_t,const vec3_t,const vec3_t,const vec3_t,int,int)){}\nstatic float TCE_WeaponWallPosturePitch(int e,int f,float p){return p;}\nstatic int TCE_WeaponWallProneBlocked(int e,float a){return 0;}\nstatic void TCE_WeaponWallCancelProneFire(playerState_t*p,usercmd_t*c,float a){}\nstatic float TCE_WeaponWallFraction(playerState_t*p,int l,void(*t)(trace_t*,const vec3_t,const vec3_t,const vec3_t,const vec3_t,int,int)){return 1;}\nstatic float TCE_WeaponWallPitch(int e,int w,float a){return 0;}\nstatic float TCE_WeaponWallAimPitch(float a,float w){return a;}\nstatic int TCE_CorpseTerminalFrame(int t,int p,float a,float b,int v){return t;}\nstatic void G_TCECorpseAnimations(entityState_t*s,int l,int t){}\nstatic float TCE_WeaponWallApproach(float a,float b,int ms){return b;}\n'
    if not args.baseline:
        bodies += '\n'+function(source('src/cgame/cg_players.c'),'CG_AnimationEndFrame')
    else:
        bodies += '\nstatic int CG_AnimationEndFrame(const animation_t*a){return a->firstFrame+a->numFrames-1;}\n'
    if 'static void CG_WeaponWallAvoidance(' in weapons:
        bodies += '\n'+function(weapons,'CG_WeaponWallAvoidance')
    else:
        bodies += '\nstatic void CG_WeaponWallAvoidance(vec3_t o,vec3_t a,int w){}\n'
    if 'static void TCE_RecordCustomWeaponIcon(' in media_source:
        bodies += '\n'+function(media_source,'TCE_RecordCustomWeaponIcon')
    else:
        bodies += '\nstatic void TCE_RecordCustomWeaponIcon(tce_weaponInfo_t *i,const char *n,int s){}\n'
    tests = r'''
int main(void){int failures=0,i;int shots[4]={1,2,3,4};tce_weaponInfo_t media={{42,0,0,0},255};
pm->pmext=&ext;
{playerState_t p={0};usercmd_t cmd={0};p.weapon=3;p.eFlags=EF_PRONE|EF_FIRING;p.weaponstate=WEAPON_FIRING;p.weaponDelay=10;p.holdable[11]=2;cmd.buttons=3;
TCE_WeaponWallCancelProneFire(&p,&cmd,.2f);
if((cmd.buttons&BUTTON_ATTACK)||(p.eFlags&EF_FIRING)||p.weaponstate!=WEAPON_READY||p.weaponDelay||p.holdable[11]){puts("FAIL prone fire cancellation");++failures;}
cmd.buttons=1;p.eFlags=0;TCE_WeaponWallCancelProneFire(&p,&cmd,1);if(cmd.buttons!=1){puts("FAIL standing attack preserved");++failures;}
if(TCE_WeaponWallPosturePitch(0,PMF_DUCKED,78)!=-78||TCE_WeaponWallPosturePitch(EF_PRONE,PMF_DUCKED,78)!=78||TCE_WeaponWallAimPitch(0,-78)!=-78){puts("FAIL crouch high-ready aim");++failures;}}
{playerState_t p={0};vec3_t mins={-18,-18,-24},maxs={18,18,48};p.weapon=3;p.viewheight=42;probeMode=1;traceCalls=0;
if(fabs(TCE_WeaponWallFraction(&p,1,CG_Trace)-.2f)>.001f||traceCalls!=3){puts("FAIL grip probe missed corner");++failures;}
probeMode=2;traceCalls=0;if(TCE_WeaponWallFraction(&p,1,CG_Trace)!=1){puts("FAIL floor treated as wall");++failures;}
p.eFlags=EF_PRONE;p.viewheight=-10;probeMode=1;traceCalls=0;TCE_WeaponWallFraction(&p,1,CG_Trace);if(traceStarts[1][2]!=-10||traceStarts[2][2]!=-10){puts("FAIL prone shoulder probes below view plane");++failures;}
probeMode=3;traceCalls=0;p.velocity[0]=100;TCE_WeaponWallProtectHead(&p,mins,maxs,1,.3f,CG_Trace);
if(fabs(p.origin[0]+12.5f)>.01f||p.velocity[0]!=0||traceCalls!=2){puts("FAIL authoritative head clearance");++failures;}
probeMode=0;}
{playerState_t p={0};int v;p.weapon=3;p.eFlags=EF_PRONE;wallFraction=1;probeMode=0;
if(TCE_WeaponWallProneTarget(&p,0,CG_Trace)!=0){puts("FAIL clear prone yaw");++failures;}
probeMode=4;if(TCE_WeaponWallProneTarget(&p,0,CG_Trace)!=30){puts("FAIL right clearance selection");++failures;}
probeMode=5;if(TCE_WeaponWallProneTarget(&p,-40,CG_Trace)!=-20){puts("FAIL left clearance selection");++failures;}probeMode=0;
p.eFlags|=EF_PRONE_MOVING;if(TCE_WeaponWallProneTarget(&p,0,CG_Trace)!=78||!TCE_WeaponWallProneStowed(p.eFlags,0)){puts("FAIL crawl stow");++failures;}
p.eFlags=EF_PRONE;wallFraction=.2f;if(TCE_WeaponWallProneTarget(&p,0,CG_Trace)!=78){puts("FAIL blocked prone stow");++failures;}
for(v=-70;v<=70;v++){int packed=TCE_WeaponWallProneState(23,(float)v);if((packed&255)!=23||fabsf(TCE_WeaponWallPosePitch(EF_PRONE,packed)-v)>.29f){puts("FAIL prone side wire");++failures;}}
if(!TCE_WeaponWallProneStowed(EF_PRONE,TCE_WeaponWallPosePitch(EF_PRONE,TCE_WeaponWallProneState(23,78)))){puts("FAIL prone stow wire");++failures;}
wallFraction=1;}
{float a=0;for(i=0;i<15;i++)a=TCE_WeaponWallApproach(a,1,16);
if(TCE_WeaponWallPitch(1,3,a)>=78.f){puts("FAIL early low-ready snap");++failures;}
a=TCE_WeaponWallApproach(a,1,16);if(TCE_WeaponWallPitch(1,3,a)!=78.f){puts("FAIL low-ready smoothing duration");++failures;}
if(TCE_WeaponWallAimPitch(60,78)!=78||TCE_WeaponWallAimPitch(-80,78)!=78||TCE_WeaponWallAimPitch(270,0)!=270){puts("FAIL low-ready aim bound or disabled preservation");++failures;}}
{float a=0;for(i=0;i<30;i++){float prev=a;a=TCE_WeaponWallApproach(a,1,16);if(a-prev>.06401f){puts("FAIL abrupt lowering");++failures;}}if(a<.95f||a>=1){puts("FAIL lowering duration");++failures;}for(i=0;i<32;i++)a=TCE_WeaponWallApproach(a,0,16);if(a!=0){puts("FAIL smooth return");++failures;}}
{entityState_t b={31,35,0};G_TCECorpseAnimations(&b,277,286);if(b.legsAnim!=277||b.torsoAnim!=286||!(b.eFlags&EF_FORCE_END_FRAME)){puts("FAIL saved custom death selection");++failures;}}
if(TCE_CorpseTerminalFrame(105,104,41,0,1)!=104||TCE_CorpseTerminalFrame(49,48,0,0,1)!=49||TCE_CorpseTerminalFrame(105,104,41,0,0)!=105){puts("FAIL corpse reset-frame selection");++failures;}
cgs.tceWeaponWallAvoidance=g_weaponWallAvoidance.integer=1;
{animation_t a={1700,42,0};if(CG_AnimationEndFrame(&a)!=1741){puts("FAIL forward terminal frame");++failures;}a.flags=4;if(CG_AnimationEndFrame(&a)!=1700){puts("FAIL reversed terminal frame");++failures;}}
for(i=0;i<2;i++){pm->cmd.serverTime=1300000000;ext.proneTime=i?-1299999700:1299999700;if(!PM_TCEAimPostureSettled()){puts("FAIL settled after long uptime");++failures;}}
for(i=0;i<2;i++){pm->cmd.serverTime=1300000000;ext.proneTime=i?-1299999900:1299999900;if(PM_TCEAimPostureSettled()){puts("FAIL posture transition guard");++failures;}}
cg.refdef_current=&camera;cg_entities[0].currentState.pos.trBase[0]=1000;
for(chosen=0;chosen<4;chosen++){echoCalls=lastEcho=0;CG_TCEFireSound855(&cg_entities[0],&media,shots);if(echoCalls!=1||lastEcho!=42){puts("FAIL missing distant variant");++failures;}}
media.flashEchoSound[0]=0;echoCalls=0;CG_TCEFireSound855(&cg_entities[0],&media,shots);if(echoCalls){puts("FAIL absent echo");++failures;}
shots[1]=0;for(i=0;i<4;i++)media.flashEchoSound[i]=42+i;
for(chosen=0;chosen<4;chosen++){echoCalls=0;CG_TCEFireSound855(&cg_entities[0],&media,shots);if(echoCalls!=1||lastEcho!=42+chosen){puts("FAIL independent distant variants");++failures;}}
{vec3_t origin={0},angles={0};camera.viewaxis[0][0]=camera.viewaxis[1][1]=camera.viewaxis[2][2]=1;
cg_weaponWallAvoidance.integer=1;cg.predictedPlayerState.weapon=3;cg.time=1000;wallFraction=.5f;cg.pmext.tceWeaponWallAmount=.5f;CG_WeaponWallAvoidance(origin,angles,3);
if(fabs(angles[0]-78.f)>.001f||fabs(origin[0]+3.f)>.001f||fabs(origin[2]+2.f)>.001f){puts("FAIL wall lowering");++failures;}
wallFraction=1;cg.pmext.tceWeaponWallAmount=0;memset(origin,0,sizeof(origin));memset(angles,0,sizeof(angles));CG_WeaponWallAvoidance(origin,angles,3);
if(angles[0]!=0||origin[0]!=0){puts("FAIL clear wall avoidance");++failures;}}
{gclient_t c={0};gentity_t e={&c};c.ps.weapon=45;c.ps.stats[1]=4;c.pmext.tceWeaponWallAmount=.5f;wallFraction=.5f;CalcMuzzlePoints(&e,45);
if(fabs(forward[2]+sinf(78.f*.01745329252f))>.0001f||c.ps.viewangles[0]!=0){puts("FAIL authoritative lowered shot direction");++failures;}
{vec3_t end;c.pmext.tceShotAngles[0]=7.f;Bullet_Endpos(&e,0,&end);
if(fabs(end[2]+8192.f*sinf(78.f*.01745329252f))>.01f){puts("FAIL actual pre-recoil bullet endpoint");++failures;}}
wallFraction=1;c.pmext.tceWeaponWallAmount=0;CalcMuzzlePoints(&e,45);if(fabs(forward[2])>.0001f){puts("FAIL clear shot direction");++failures;}
c.ps.weapon=60;c.pmext.tceWeaponWallAmount=1;wallFraction=0;CalcMuzzlePoints(&e,60);if(fabs(forward[2])>.0001f){puts("FAIL mounted direction");++failures;}}
g_weaponWallAvoidance.integer=0;{gclient_t c={0};gentity_t e={&c};c.ps.weapon=45;c.pmext.tceWeaponWallAmount=1;CalcMuzzlePoints(&e,45);if(forward[2]!=0){puts("FAIL server disabled");++failures;}}
TCE_RecordCustomWeaponIcon(&tce_cg_weapons[3],"icons/custom_rifle",83);
if(tce_customWeaponIcons[3]!=83){puts("FAIL custom artwork selection");++failures;}
TCE_RecordCustomWeaponIcon(&tce_cg_weapons[3],"icons/iconw_luger_1",84);
if(tce_customWeaponIcons[3]!=0){puts("FAIL stock placeholder filtering");++failures;}
printf("%d failures\n",failures);return failures?1:0;}
'''
    generated=out/'regression.c';generated.write_text(prelude+bodies+tests,newline='\n')
    env,compiler=msvc_environment(out,args.arch,'14.44')
    exe=out/'regression.exe'
    command=[compiler,'/nologo','/TC','/O2','/W3',str(generated),'/Fe:'+str(exe),'/Fo:'+str(out/'regression.obj')]
    if args.sanitize:command+=['/fsanitize=address','/Zi','/link','/DEBUG']
    built=subprocess.run(command,env=env,cwd=out,capture_output=True,text=True)
    (out/'compile.log').write_text(built.stdout+built.stderr)
    if built.returncode:print(built.stdout+built.stderr);return built.returncode
    return subprocess.run([str(exe)],cwd=out,env=env).returncode

if __name__=='__main__':raise SystemExit(main())
