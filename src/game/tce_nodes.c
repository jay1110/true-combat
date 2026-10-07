#include "g_local.h"
#include "tce_nodes.h"
extern vmCvar_t g_developer;

tceNode_t tceNodes[TCE_MAX_NODES];
short tceNumNodes;
int tceLastNode=-1;

/* SaveNodes200409a0 / Linux00078cde: version2 field stream, not a struct dump. */
void TCE_SaveNodes(const char *filename) {
    fileHandle_t file;
    vmCvar_t map;
    float version=2.0f;
    int i;
    short j;
    trap_FS_FOpenFile(filename,&file,FS_WRITE);
    if(!file){G_Printf("^1ERROR: Can't open waypoint file\n");return;}
    trap_Cvar_Register(&map,"mapname","",0x44);
    /* Original copies map.string to an unused64-byte local. Do not reproduce
     * that possible overflow: the mapname is not present in the file format. */
    trap_FS_Write("truecombat",sizeof("truecombat"),file);
    trap_FS_Write(&version,4,file);
    trap_FS_Write(&tceNumNodes,2,file);
    for(i=0;i<tceNumNodes;i++) {
        tceNode_t *node=&tceNodes[i];
        trap_FS_Write(&node->number,2,file);
        trap_FS_Write(node->origin,12,file);
        trap_FS_Write(&node->flags,2,file);
        trap_FS_Write(node->entities,6,file);
        trap_FS_Write(&node->packed,4,file);
        trap_FS_Write(&node->numLinks,2,file);
        for(j=0;j<node->numLinks;j++) {
            trap_FS_Write(&node->links[j].target,2,file);
            trap_FS_Write(&node->links[j].flags,2,file);
        }
    }
    trap_FS_FCloseFile(file);
    G_Printf("Successfully wrote waypoint file %s\n",filename);
}

/* ConnectClosestNodes20041560 / Linux0007a064. The original
 * iterates only through numNodes-2, with an inclusive initial link threshold. */
void TCE_ConnectClosestNodes(int maxLinks) {
    int from,to,k,closest;
    vec3_t delta,mins={-16,-16,-16},maxs={16,16,16};
    float distance,best;
    double returnedDistance;
    trace_t trace;
    qboolean connected;
    for(from=0;from<tceNumNodes-1;from++) {
        tceNode_t *node=&tceNodes[from];
        if(node->flags==-1 || node->numLinks>maxLinks)continue;
        closest=-1;best=-1.0f;
        for(to=0;to<tceNumNodes-1;to++) {
            if(to==from || tceNodes[to].flags==-1)continue;
            connected=qfalse;
            for(k=0;k<node->numLinks;k++)if(node->links[k].target==to)connected=qtrue;
            if(connected)continue;
            VectorSubtract(node->origin,tceNodes[to].origin,delta);
            returnedDistance=VectorNormalize(delta);
            distance=(float)returnedDistance;
            if(!(returnedDistance<=48.0))continue;
            trap_Trace(&trace,node->origin,mins,maxs,tceNodes[to].origin,ENTITYNUM_NONE,0x10001);
            if(trace.fraction==1.0f && (distance<best || best==-1.0f)) {
                best=distance;closest=to;
            }
        }
        if((short)closest!=-1 && best!=-1.0f) {
            if(TCE_ConnectNodes(from,(short)closest,0))
                G_Printf("Created path between node %d and node %d\n",from,(int)(short)closest);
            TCE_ConnectNodes((short)closest,from,0);
        }
    }
}

/* ClearNodes20040190 / Linux0007829c. Preserve original unwritten padding. */
void TCE_ClearNodes(void) {
    int i,j;
    for(i=0;i<TCE_MAX_NODES;i++) {
        tceNode_t *n=&tceNodes[i];
        VectorClear(n->origin);n->number=n->flags=-1;
        n->entities[0]=n->entities[1]=n->entities[2]=ENTITYNUM_NONE;
        n->packed=0;n->team=n->moverType=n->numLinks=0;
        n->moverEntity=ENTITYNUM_NONE;n->scratch=0;
        for(j=0;j<TCE_MAX_NODE_LINKS;j++){n->links[j].target=-1;n->links[j].flags=0;}
    }
    tceLastNode=-1;tceNumNodes=0;
}
/* AddNode20040040 / Linux000780b0. Windows masks moverEntity to nine bits. */
qboolean TCE_AddNode(const vec3_t origin,short flags,const short entities[3],int packed) {
    tceNode_t *n;
    if(tceNumNodes+1>TCE_MAX_NODES)return qfalse;
    n=&tceNodes[tceNumNodes];VectorCopy(origin,n->origin);n->number=tceNumNodes;
    n->flags=flags;memcpy(n->entities,entities,sizeof(n->entities));n->packed=packed;
    n->moverEntity=(packed>>7)&511;n->moverType=(packed>>2)&3;n->team=(packed&1)?1:2;
    tceNumNodes++;return qtrue;
}
/* ConnectNodes20040120 / Linux000781f2. */
qboolean TCE_ConnectNodes(int from,int to,short flags) {
    tceNode_t *n=&tceNodes[from];
    if(n->numLinks+1<13 && n->flags!=-1 && to!=-1) {
        n->links[n->numLinks].target=(short)to;
        n->links[n->numLinks].flags=(unsigned short)flags;n->numLinks++;return qtrue;
    }
    return qfalse;
}
/* LoadNodes20040700 / Linux000788c6. The file's packed field is deliberately
 * read over the short packed word and first entity word, exactly as the original
 * stack layout does. Only the signed low word is passed to AddNode. */
qboolean TCE_LoadNodes(const char *filename) {
    fileHandle_t file;vmCvar_t map;char magic[64]={0};float version;
    short count,number,flags,links,to,linkFlags;int i,j;
    short packedAndEntities[4]={0,0,0,0};vec3_t origin;
    trap_FS_FOpenFile(filename,&file,FS_READ);
    if(!file){G_Printf("^1WARNING: Server failed to load %s\n",filename);return qfalse;}
    trap_Cvar_Register(&map,"mapname","",0x44);
    trap_FS_Read(magic,sizeof("truecombat"),file);
    if(Q_stricmp(magic,"truecombat")) {
        G_Printf("^1WARNING: Incompatible or corrupt waypoint file, load aborted\n");
        trap_FS_FCloseFile(file);return qfalse;
    }
    trap_FS_Read(&version,4,file);
    if(version!=2.0f && !g_developer.integer) {
        G_Printf("^1WARNING: Waypoint file is wrong version, load aborted\n");
        trap_FS_FCloseFile(file);return qfalse;
    }
    trap_FS_Read(&count,2,file);
    for(i=0;i<count;i++) {
        trap_FS_Read(&number,2,file);trap_FS_Read(origin,12,file);
        trap_FS_Read(&flags,2,file);trap_FS_Read(packedAndEntities+1,6,file);
        trap_FS_Read(packedAndEntities,4,file);trap_FS_Read(&links,2,file);
        TCE_AddNode(origin,flags,packedAndEntities+1,(int)packedAndEntities[0]);
        for(j=0;j<links;j++) {
            trap_FS_Read(&to,2,file);trap_FS_Read(&linkFlags,2,file);
            TCE_ConnectNodes(number,to,linkFlags);
        }
    }
    trap_FS_FCloseFile(file);return qtrue;
}
/* InitNodes20040210 / Linux00078350. */
void TCE_InitNodes(void) {
    fileHandle_t file;vmCvar_t map;char path[MAX_QPATH];
    TCE_ClearNodes();trap_Cvar_Register(&map,"mapname","",0x44);
    Com_sprintf(path,sizeof(path),"botroutes/%s.wps",map.string);
    trap_FS_FOpenFile(path,&file,FS_READ);
    if(!file){G_Printf("Couldn't load waypoints for map %s\n",map.string);return;}
    trap_FS_FCloseFile(file);TCE_LoadNodes(path);
}
/* UpdateNodes200412d0 / Linux00079ca8. Real loaded node state, not a dummy
 * callback: clear removed dynamic restrictions and opened obstructed links. */
void TCE_UpdateNodes(void) {
    int i,j;trace_t tr;
    for(i=0;i<tceNumNodes;i++) {
        tceNode_t *n=&tceNodes[i];
        if(n->flags&0x30)for(j=0;j<3;j++) {
            int e=n->entities[j];
            if(e!=ENTITYNUM_NONE && e!=0 && !g_entities[e].spawnflags)n->flags&=~0x30;
        }
        if((n->flags&4)&&!n->moverType&&!g_entities[n->moverEntity].spawnflags)n->flags&=~4;
        for(j=0;j<n->numLinks;j++)if(n->links[j].flags&0x10) {
            vec3_t start,end;VectorCopy(n->origin,start);
            VectorCopy(tceNodes[n->links[j].target].origin,end);
            trap_Trace(&tr,start,NULL,NULL,end,ENTITYNUM_NONE,0x10001);
            if(tr.fraction==1.0f)n->links[j].flags&=~0x10;
        }
    }
}
