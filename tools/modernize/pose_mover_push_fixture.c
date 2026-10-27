/* Complete actual server push body is extracted here by the focused runner.
 * Only collision returns an empty world: this compares the successful player
 * movement arithmetic, not blocked pushing, gameplay physics or BSP behavior. */
#include "src/game/g_local.h"
#include <stdlib.h>
#define PUSH_STACK_DEPTH 3
int pushedStackDepth;
typedef struct {
    gentity_t *ent;
    vec3_t origin,angles;
    float deltayaw;
} pushed_t;
pushed_t pushed[MAX_GENTITIES*PUSH_STACK_DEPTH],*pushed_p;
gentity_t *G_TestEntityPosition(gentity_t *ent) {(void)ent;return NULL;}
void QDECL G_Error(const char *format,...) {(void)format;abort();}

/* ACTUAL_PUSH */

int FixturePushedPosition(const vec3_t target,const vec3_t newPivot,
    const vec3_t move,const vec3_t amove,vec3_t out) {
    gentity_t check,pusher;gclient_t client;vec3_t translation,rotation;
    memset(&check,0,sizeof(check));memset(&pusher,0,sizeof(pusher));memset(&client,0,sizeof(client));
    check.client=&client;check.s.groundEntityNum=pusher.s.number=200;
    VectorCopy(target,check.s.pos.trBase);VectorCopy(target,client.ps.origin);
    VectorCopy(newPivot,pusher.r.currentOrigin);VectorCopy(move,translation);VectorCopy(amove,rotation);
    pushed_p=pushed;pushedStackDepth=0;
    if(!G_TryPushingEntity(&check,&pusher,translation,rotation))return 0;
    VectorCopy(client.ps.origin,out);return 1;
}
