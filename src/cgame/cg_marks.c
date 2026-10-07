// cg_marks.c -- wall marks

#include "cg_local.h"

/*
===================================================================

MARK POLYS

===================================================================
*/


markPoly_t	cg_activeMarkPolys;			// double linked list
markPoly_t	*cg_freeMarkPolys;			// single linked list
markPoly_t	cg_markPolys[MAX_MARK_POLYS];

/*
===================
CG_InitMarkPolys

This is called at startup and for tournement restarts
===================
*/
void	CG_InitMarkPolys( void ) {
	int		i;
	markPoly_t *trav, *lasttrav;

	memset( cg_markPolys, 0, sizeof(cg_markPolys) );

	cg_activeMarkPolys.nextMark = &cg_activeMarkPolys;
	cg_activeMarkPolys.prevMark = &cg_activeMarkPolys;
	cg_freeMarkPolys = cg_markPolys;
	for ( i = 0, trav = cg_markPolys+1, lasttrav = cg_markPolys ; i < MAX_MARK_POLYS - 1 ; i++, trav++ ) {
		lasttrav->nextMark = trav;
		lasttrav = trav;
	}
}


/*
==================
CG_FreeMarkPoly
==================
*/
void CG_FreeMarkPoly( markPoly_t *le ) {
	if ( !le->prevMark ) {
		CG_Error( "CG_FreeLocalEntity: not active" );
	}

	// remove from the doubly linked active list
	le->prevMark->nextMark = le->nextMark;
	le->nextMark->prevMark = le->prevMark;

	// the free list is only singly linked
	le->nextMark = cg_freeMarkPolys;
	cg_freeMarkPolys = le;
}

/*
===================
CG_AllocMark

Will allways succeed, even if it requires freeing an old active mark
===================
*/
markPoly_t	*CG_AllocMark( int endTime ) {
	markPoly_t	*le;//, *trav, *lastTrav;
	int time;

	if ( !cg_freeMarkPolys ) {
		// no free entities, so free the one at the end of the chain
		// remove the oldest active entity
		time = cg_activeMarkPolys.prevMark->time;
		while (cg_activeMarkPolys.prevMark && time == cg_activeMarkPolys.prevMark->time) {
			CG_FreeMarkPoly( cg_activeMarkPolys.prevMark );
		}
	}

	le = cg_freeMarkPolys;
	cg_freeMarkPolys = cg_freeMarkPolys->nextMark;

	memset( le, 0, sizeof( *le ) );

	// Ridah, TODO: sort this, so the list is always sorted by longest duration -> shortest duration,
	// this way the shortest duration mark will always get overwritten first
	//for (trav = cg_activeMarkPolys.nextMark; (trav->duration + trav->time > endTime) && (trav != cg_activeMarkPolys.prevMark) ; lastTrav = trav, trav++ ) {
		// Respect the FOR loop
	//}

	// link into the active list
	le->nextMark = cg_activeMarkPolys.nextMark;
	le->prevMark = &cg_activeMarkPolys;
	cg_activeMarkPolys.nextMark->prevMark = le;
	cg_activeMarkPolys.nextMark = le;
	return le;
}



/*
CG_ImpactMark()
projection is a normal and distance (not a plane, but rather how far to project)
it MUST be normalized!
if lifeTime < 0, then generate a temporary mark
*/

// Ridah, increased this since we leave them around for longer
#define	MAX_MARK_FRAGMENTS	384
#define	MAX_MARK_POINTS		1024
//#define	MAX_MARK_FRAGMENTS	128
//#define	MAX_MARK_POINTS		384

// these are ignored now for the most part
//#define	MARK_TOTAL_TIME		20000	// (SA) made this a cvar: cg_markTime  (we could cap the time or remove marks quicker if too long a time starts to cause new marks to not appear)
#define	MARK_FADE_TIME		10000

// comment out to use old-style mark code
#define YDNAR_DECAL_MARKS

void CG_ImpactMark( qhandle_t markShader, vec3_t origin, vec4_t projection, float radius, float orientation, float r, float g, float b, float a, int lifeTime )
#ifdef YDNAR_DECAL_MARKS
{
	int			i;
	vec3_t		pushedOrigin, axis[ 3 ];
	vec4_t		color;
	int			fadeTime;
	vec3_t		points[ 4 ];
	float baseRotation = 90;
	
	
	/* early out */
	if( lifeTime == 0 )
		return;
	
	/* set projection (inverse of dir) */
 	//%	VectorSubtract( vec3_origin, dir, projection );
	//%	VectorNormalize( projection );
	//%	projection[ 3 ] = radius * 8;
	
	/* make rotated polygon axis */
	VectorNormalize2( projection, axis[ 0 ] );
	PerpendicularVector( axis[ 1 ], axis[ 0 ] );
	/* TC:E aligns wall marks vertically before applying the shot rotation. */
	if (fabs(axis[0][2]) < (double)0.707f) {
		RotatePointAroundVector(axis[2], axis[0], axis[1], 90);
		if (axis[2][2] > -0.7f) {
			baseRotation = 270;
			RotatePointAroundVector(axis[2], axis[0], axis[1], baseRotation);
		}
		if (axis[2][2] > -0.7f) {
			baseRotation = 0;
			RotatePointAroundVector(axis[2], axis[0], axis[1], baseRotation);
		}
		if (axis[2][2] > -0.7f) baseRotation = 180;
	}
	RotatePointAroundVector( axis[ 2 ], axis[ 0 ], axis[ 1 ], baseRotation + orientation );
	CrossProduct( axis[ 0 ], axis[ 2 ], axis[ 1 ] );
	
	/* push the origin out a bit */
	VectorMA( origin, -1.0f, axis[ 0 ], pushedOrigin );
	
	/* create the full polygon */
	for( i = 0; i < 3; i++ )
	{
		/* old */
		//%	points[ 0 ][ i ] = pushedOrigin[ i ] - radius * axis[ 1 ][ i ] - radius * axis[ 2 ][ i ];
		//%	points[ 1 ][ i ] = pushedOrigin[ i ] + radius * axis[ 1 ][ i ] - radius * axis[ 2 ][ i ];
		//%	points[ 2 ][ i ] = pushedOrigin[ i ] + radius * axis[ 1 ][ i ] + radius * axis[ 2 ][ i ];
		//%	points[ 3 ][ i ] = pushedOrigin[ i ] - radius * axis[ 1 ][ i ] + radius * axis[ 2 ][ i ];
		
		/* new */
		{
			double side = (double)radius * axis[1][i], other = (double)radius * axis[2][i];
			double low = pushedOrigin[i] - side;
			points[0][i] = (float)(low - other);
			points[1][i] = (float)(low + other);
			points[2][i] = (float)(((double)axis[1][i] + axis[2][i]) * radius + pushedOrigin[i]);
			points[3][i] = (float)(side + pushedOrigin[i] - other);
		}
	}
	
	/* debug code */
	#if 0
		VectorSet( points[ 0 ], origin[ 0 ] - radius, origin[ 1 ] - radius, origin[ 2 ] );
		VectorSet( points[ 1 ], origin[ 0 ] - radius, origin[ 1 ] + radius, origin[ 2 ] );
		VectorSet( points[ 2 ], origin[ 0 ] + radius, origin[ 1 ] + radius, origin[ 2 ] );
		VectorSet( points[ 3 ], origin[ 0 ] + radius, origin[ 1 ] - radius, origin[ 2 ] );
		CG_Printf( "Dir: %f %f %f\n", dir[ 0 ], dir[ 1 ], dir[ 2 ] );
	#endif
	
	/* set color */
	color[ 0 ] = r;
	color[ 1 ] = g;
	color[ 2 ] = b;
	color[ 3 ] = a;
	
	/* set decal times (in seconds) */
	fadeTime = lifeTime >> 4;
	
	/* add the decal */
	trap_R_ProjectDecal( markShader, 4, points, projection, color, lifeTime, fadeTime );
}
#else
{
	vec3_t			axis[3];
	float			texCoordScale;
	vec3_t			originalPoints[4];
	byte			colors[4];
	int				i, j;
	int				numFragments;
	markFragment_t	markFragments[MAX_MARK_FRAGMENTS], *mf;
	vec5_t			markPoints[MAX_MARK_POINTS];	// Ridah, made it vec5_t so it includes S/T
	vec3_t			projection;
	int				multMaxFragments=1;

	if ( !cg_markTime.integer ) {
		return;
	}

	if( radius <= 0 )
		return;
	
	if( temporary ) {
		if(CG_CullPointAndRadius( origin, radius )) {
			return;
		}
	}

	// Ridah, if no duration, use the default
	if (duration < 0) {
		if (duration == -2) {
			multMaxFragments = -1;	// use original mapping
		}

//		duration = MARK_TOTAL_TIME;
		duration = cg_markTime.integer;
	}

	// create the texture axis
	VectorNormalize2( dir, axis[0] );
	PerpendicularVector( axis[1], axis[0] );
	RotatePointAroundVector( axis[2], axis[0], axis[1], orientation );
	CrossProduct( axis[0], axis[2], axis[1] );

	texCoordScale = 0.5 * 1.0 / radius;

	// create the full polygon
	for ( i = 0 ; i < 3 ; i++ ) {
		originalPoints[0][i] = origin[i] - radius * axis[1][i] - radius * axis[2][i];
		originalPoints[1][i] = origin[i] + radius * axis[1][i] - radius * axis[2][i];
		originalPoints[2][i] = origin[i] + radius * axis[1][i] + radius * axis[2][i];
		originalPoints[3][i] = origin[i] - radius * axis[1][i] + radius * axis[2][i];
	}
	
	// get the fragments
	//VectorScale( dir, -20, projection );
	VectorScale( dir, radius*2, projection );
	numFragments = trap_CM_MarkFragments( (int)orientation, (void *)originalPoints,
					projection, MAX_MARK_POINTS, (float *)&markPoints[0],
					MAX_MARK_FRAGMENTS*multMaxFragments, markFragments );

	colors[0] = red * 255;
	colors[1] = green * 255;
	colors[2] = blue * 255;
	colors[3] = alpha * 255;

	for ( i = 0, mf = markFragments ; i < numFragments ; i++, mf++ ) {
		polyVert_t	*v;
		polyVert_t	verts[MAX_VERTS_ON_POLY];
		markPoly_t	*mark;
		qboolean	hasST;

		// we have an upper limit on the complexity of polygons
		// that we store persistantly
		if ( mf->numPoints > MAX_VERTS_ON_POLY ) {
			mf->numPoints = MAX_VERTS_ON_POLY;
		}
		if (mf->numPoints < 0) {
			hasST = qtrue;
			mf->numPoints *= -1;
		} else {
			hasST = qfalse;
		}
		for ( j = 0, v = verts ; j < mf->numPoints ; j++, v++ ) {
			vec3_t		delta;

			VectorCopy( markPoints[mf->firstPoint + j], v->xyz );

			if (!hasST) {
				VectorSubtract( v->xyz, origin, delta );
				v->st[0] = 0.5 + DotProduct( delta, axis[1] ) * texCoordScale;
				v->st[1] = 0.5 + DotProduct( delta, axis[2] ) * texCoordScale;
			} else {
				v->st[0] = markPoints[mf->firstPoint + j][3];
				v->st[1] = markPoints[mf->firstPoint + j][4];
			}

			*(int *)v->modulate = *(int *)colors;
		}

		// if it is a temporary (shadow) mark, add it immediately and forget about it
		if ( temporary ) {
			trap_R_AddPolyToScene( markShader, mf->numPoints, verts );
			continue;
		}

		// otherwise save it persistantly
		mark = CG_AllocMark(cg.time + duration);
		mark->time = cg.time;
		mark->alphaFade = alphaFade;
		mark->markShader = markShader;
		mark->poly.numVerts = mf->numPoints;
		mark->color[0] = red;
		mark->color[1] = green;
		mark->color[2] = blue;
		mark->color[3] = alpha;
		mark->duration = duration;
		memcpy( mark->verts, verts, mf->numPoints * sizeof( verts[0] ) );
	}
}
#endif



/*
===============
CG_AddMarks
===============
*/

/* TC:E 3004bbd0: the legacy fragment API is still used for bullet marks.
 * Its twelve arguments are deliberately separate from CG_ImpactMark's decal API. */
void CG_EliteImpactMark(qhandle_t shader, vec3_t origin, vec3_t dir,
                       float orientation, float red, float green, float blue,
                       float alpha, qboolean alphaFade, float radius,
                       qboolean temporary, int duration) {
    vec3_t axis[3], points[4], projection;
    vec5_t markPoints[MAX_MARK_POINTS];
    markFragment_t fragments[MAX_MARK_FRAGMENTS];
    polyVert_t verts[MAX_VERTS_ON_POLY];
    byte colors[4];
    float rotation = 90, scale;
    int i, j, count, multiplier = 1;
    qboolean fallback = qfalse;
    if (!cg_markTime.integer) return;
    if (radius <= 0) CG_Error("CG_ImpactMark called with <= 0 radius");
    if (duration < 0) {
        if (duration == -2) multiplier = -1;
        duration = cg_markTime.integer;
    }
    VectorNormalize2(dir, axis[0]);
    PerpendicularVector(axis[1], axis[0]);
    if (fabs(axis[0][2]) < (double)0.707f) {
        RotatePointAroundVector(axis[2], axis[0], axis[1], 90);
        if (axis[2][2] > -0.7f) {
            rotation = 270;
            RotatePointAroundVector(axis[2], axis[0], axis[1], rotation);
        }
        if (axis[2][2] > -0.7f) {
            rotation = 0;
            RotatePointAroundVector(axis[2], axis[0], axis[1], rotation);
        }
        if (axis[2][2] > -0.7f) rotation = 180;
    }
    RotatePointAroundVector(axis[2], axis[0], axis[1], rotation + orientation);
    CrossProduct(axis[0], axis[2], axis[1]);
    scale = (float)(0.5 / radius);
    for (i = 0; i < 3; ++i) {
        float low = (float)((double)origin[i] - (double)radius * axis[1][i]);
        float side = radius * axis[2][i];
        points[0][i] = low - side;
        points[1][i] = (float)((double)radius * axis[1][i] + origin[i] - side);
        points[2][i] = (float)(((double)axis[1][i] + axis[2][i]) * radius + origin[i]);
        points[3][i] = side + low;
        projection[i] = (float)((double)dir[i] * (temporary ? 4.5 : (double)radius * .25 + 2.0));
    }
    count = trap_CM_MarkFragments((int)orientation, points, projection,
              MAX_MARK_POINTS, (float *)markPoints, MAX_MARK_FRAGMENTS * multiplier, fragments);
    if (!count && !temporary) {
        VectorScale(dir, -20, projection);
        count = trap_CM_MarkFragments((int)orientation, points, projection,
                  MAX_MARK_POINTS, (float *)markPoints, MAX_MARK_FRAGMENTS * multiplier, fragments);
        fallback = qtrue;
    }
    colors[0] = (byte)(int)((double)red * 255);
    colors[1] = (byte)(int)((double)green * 255);
    colors[2] = (byte)(int)((double)blue * 255);
    colors[3] = (byte)(int)((double)alpha * 255);
    for (i = 0; i < count; ++i) {
        markFragment_t *mf = &fragments[i];
        qboolean hasST;
        markPoly_t *mark;
        if (mf->numPoints > MAX_VERTS_ON_POLY) mf->numPoints = MAX_VERTS_ON_POLY;
        if (fallback && mf->numPoints < 0) continue;
        hasST = mf->numPoints < 0;
        if (hasST) mf->numPoints = -mf->numPoints;
        for (j = 0; j < mf->numPoints; ++j) {
            float *p = markPoints[mf->firstPoint + j];
            VectorCopy(p, verts[j].xyz);
            if (!hasST || fallback) {
                double x = (double)p[0] - origin[0], y = (double)p[1] - origin[1], z = (double)p[2] - origin[2];
                verts[j].st[0] = (float)((axis[1][0]*x + axis[1][1]*y + axis[1][2]*z)*scale + .5);
                verts[j].st[1] = (float)((axis[2][0]*x + axis[2][1]*y + axis[2][2]*z)*scale + .5);
            } else {
                float sign = dir[0] > 0 || dir[1] < 0 ? -1.f : 1.f;
                verts[j].st[0] = sign * p[4];
                verts[j].st[1] = sign * p[3];
            }
            memcpy(verts[j].modulate, colors, 4);
        }
        if (temporary) {
            trap_R_AddPolyToScene(shader, mf->numPoints, verts);
            continue;
        }
        mark = CG_AllocMark(cg.time + duration);
        mark->time = cg.time; mark->alphaFade = alphaFade; mark->markShader = shader;
        mark->poly.numVerts = mf->numPoints; mark->duration = duration;
        mark->color[0] = red; mark->color[1] = green; mark->color[2] = blue; mark->color[3] = alpha;
        memcpy(mark->verts, verts, mf->numPoints * sizeof(verts[0]));
    }
}

void CG_AddMarks( void ) {
	int			j;
	markPoly_t	*mp, *next;
	int			t;
	int			fade;

	if ( !cg_markTime.integer ) {
		return;
	}

	mp = cg_activeMarkPolys.nextMark;
	for ( ; mp != &cg_activeMarkPolys ; mp = next ) {
		// grab next now, so if the local entity is freed we
		// still have it
		next = mp->nextMark;

		// see if it is time to completely remove it
		if ( cg.time > mp->time + mp->duration ) {
			CG_FreeMarkPoly( mp );
			continue;
		}

		// fade all marks out with time
		t = mp->time + mp->duration - cg.time;
		if ( t < (float)mp->duration/2.0 ) {
			fade = (int)(255.0 * (float)t / ((float)mp->duration/2.0));
			if ( mp->alphaFade ) {
				for ( j = 0 ; j < mp->poly.numVerts ; j++ ) {
					mp->verts[j].modulate[3] = fade;
				}
			} else {
				for ( j = 0 ; j < mp->poly.numVerts ; j++ ) {
					mp->verts[j].modulate[0] = mp->color[0] * fade;
					mp->verts[j].modulate[1] = mp->color[1] * fade;
					mp->verts[j].modulate[2] = mp->color[2] * fade;
				}
			}
		}

		trap_R_AddPolyToScene( mp->markShader, mp->poly.numVerts, mp->verts );
	}
}

