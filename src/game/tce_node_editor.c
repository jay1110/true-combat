#include "g_local.h"
#include "tce_nodes.h"
#include "tce_node_editor.h"

/* Windows20041450: return is a signed short; high EAX bits are unspecified. */
short TCE_FindClosestNodeToPoint(gentity_t *ent, const vec3_t point) {
    int i;
    short closest=-1;
    float best=-1.0f;
    for(i=0;i<tceNumNodes;i++) {
        tceNode_t *node=&tceNodes[i];
        vec3_t delta;
        float distance;
        trace_t tr;
        if(node->flags==-1 ||
           (ent->client->sess.sessionTeam==TEAM_AXIS && (node->flags&0x10)) ||
           (ent->client->sess.sessionTeam==TEAM_ALLIES && (node->flags&0x20)))continue;
        VectorSubtract(node->origin,point,delta);
        distance=VectorNormalize(delta);
        trap_Trace(&tr,point,NULL,NULL,node->origin,ent->s.number,0x10001);
        if(tr.fraction==1.0f && (distance<best || best==-1.0f)) {
            best=distance;closest=(short)i;
        }
    }
    return closest;
}

static void nodeBox(int index,float radius,int highlighted) {
    vec3_t mins,maxs;
    gentity_t *event;
    int axis;
    for(axis=0;axis<3;axis++) {
        mins[axis]=tceNodes[index].origin[axis]-radius;
        maxs[axis]=tceNodes[index].origin[axis]+radius;
    }
    if(tceNodes[index].flags)maxs[2]+=radius*1.5f;
    if(highlighted)for(axis=0;axis<3;axis++){mins[axis]+=1.0f;maxs[axis]+=1.0f;}
    event=G_TempEntity(mins,0x3d);
    VectorCopy(maxs,event->s.origin2);
    event->s.dmgFlags=1;event->s.effect3Time=highlighted;
}

static void nodeLine(int from,int to,int highlighted) {
    gentity_t *event=G_TempEntity(tceNodes[from].origin,0x3d);
    int axis;
    VectorCopy(tceNodes[to].origin,event->s.origin2);
    if(highlighted)for(axis=0;axis<3;axis++) {
        event->s.origin2[axis]+=1.0f;
        event->s.origin[axis]+=1.0f;
    }
    event->s.dmgFlags=0;event->s.effect3Time=highlighted;
}

/* G_ShowNode20041760: root, outgoing links and one more link level. */
void TCE_ShowNode(int index,int highlighted) {
    int i,j;
    /* The original reads outside its array for the no-node sentinel. Keep an
     * empty editor safe without manufacturing a waypoint or event. */
    if(index<0 || index>=tceNumNodes)return;
    nodeBox(index,8.0f,highlighted);
    if(highlighted)G_Printf("Pointing at node %d with flags %d\n",index,(int)tceNodes[index].flags);
    for(i=0;i<tceNodes[index].numLinks;i++) {
        int next=tceNodes[index].links[i].target;
        if(next<0 || next>=tceNumNodes)continue;
        nodeLine(index,next,highlighted);nodeBox(next,4.0f,highlighted);
        for(j=0;j<tceNodes[next].numLinks;j++) {
            int end=tceNodes[next].links[j].target;
            if(end<0 || end>=tceNumNodes)continue;
            nodeLine(next,end,highlighted);nodeBox(end,2.0f,highlighted);
        }
    }
}
