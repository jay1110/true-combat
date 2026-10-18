/* Classic ET has an embedded 8192-vertex array immediately before its decal
 * projectors. Raising r_maxpolyverts does not move that array. Preserve the
 * user's cvars and bound submissions from both native modules instead.
 * UI + cgame quotas deliberately sum to the physical capacities. */
#ifndef TCE_RENDERER_LIMITS_H
#define TCE_RENDERER_LIMITS_H
#define TCE_CLASSIC_CGAME_VERTS 7168
#define TCE_CLASSIC_UI_VERTS    1024
#define TCE_CLASSIC_CGAME_POLYS 3584
#define TCE_CLASSIC_UI_POLYS    512

typedef struct {
	int classic, initialized, frameSeen, frame;
	int vertices, polygons;
} tcePolyBudget_t;

static void TCE_PolyBudgetInit(tcePolyBudget_t *budget, const char *version)
{
	memset(budget, 0, sizeof(*budget));
	budget->classic = strncmp(version, "ET 2.", 5) == 0;
	budget->initialized = 1;
}

static void TCE_PolyBudgetFrame(tcePolyBudget_t *budget, int frame)
{
	/* UI_REFRESH can run twice in one engine frame. Stereo/loading refreshes
	 * must not each receive another quota. Retaining a quota for an unchanged
	 * time is conservative when the renderer actually starts a new frame. */
	if (budget->frameSeen && budget->frame != frame) {
		budget->vertices = budget->polygons = 0;
	}
	budget->frame = frame;
	budget->frameSeen = 1;
}

static int TCE_PolyBudgetTake(tcePolyBudget_t *budget, int numVerts, int numPolys,
	int vertexLimit, int polygonLimit)
{
	int count;
	if (numVerts <= 0 || numPolys < 0) return 0;
	if (!budget->classic) return 1;
	/* numPolys=0 denotes a 2D vertex batch, which uses the SAME vertex array
	 * but no srfPoly slot. Divide before multiplying to avoid signed overflow. */
	count = numPolys ? numPolys : 1;
	if (numPolys > polygonLimit - budget->polygons ||
	    numVerts > (vertexLimit - budget->vertices) / count) return 0;
	budget->vertices += numVerts * count;
	budget->polygons += numPolys;
	return 1;
}
#endif
