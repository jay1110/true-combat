#ifndef TCE_NODES_H
#define TCE_NODES_H
#define TCE_MAX_NODES 1024
#define TCE_MAX_NODE_LINKS 12
/* Original node stride0x6c. Reserved bytes are retained, not fabricated state. */
typedef struct { short target; unsigned short flags; } tceNodeLink_t;
typedef struct {
    vec3_t origin;
    short number, flags, entities[3], reserved16;
    int packed;
    short team, moverType, moverEntity, numLinks;
    tceNodeLink_t links[TCE_MAX_NODE_LINKS];
    byte reserved54[20];
    int scratch;
} tceNode_t;
extern tceNode_t tceNodes[TCE_MAX_NODES];
extern short tceNumNodes;
extern int tceLastNode;
void TCE_ClearNodes(void);
qboolean TCE_AddNode(const vec3_t origin, short flags, const short entities[3], int packed);
qboolean TCE_ConnectNodes(int from, int to, short flags);
qboolean TCE_LoadNodes(const char *filename);
void TCE_SaveNodes(const char *filename);
void TCE_ConnectClosestNodes(int maxLinks);
void TCE_InitNodes(void);
void TCE_UpdateNodes(void);
#endif
