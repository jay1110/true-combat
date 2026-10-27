/* Frozen pre-fix SetMoverState, source SHA256
 * 85b98349db1d3fdd93d7003eaee3ddce795de5c5e04e54fd34f4ac3c4ed587cf.
 * Test-only independent construction and callback parity oracle. */
static void Baseline_SetMoverState(gentity_t *ent, moverState_t moverState, int time)
{
	vec3_t   delta;
	float    f;
	qboolean kicked = qfalse, soft = qfalse;

	kicked = (qboolean)(ent->flags & FL_KICKACTIVATE);
	soft   = (qboolean)(ent->flags & FL_SOFTACTIVATE);  //----(SA)	added

	ent->moverState    = moverState;
	ent->s.pos.trTime  = time;
	ent->s.apos.trTime = time;
	switch (moverState)
	{
	case MOVER_POS1:
		VectorCopy(ent->pos1, ent->s.pos.trBase);
		ent->s.pos.trType = TR_STATIONARY;
		ent->active       = qfalse;
		break;
	case MOVER_POS2:
		VectorCopy(ent->pos2, ent->s.pos.trBase);
		ent->s.pos.trType = TR_STATIONARY;
		break;

	// JOSEPH 1-26-00
	case MOVER_POS3:
		VectorCopy(ent->pos3, ent->s.pos.trBase);
		ent->s.pos.trType = TR_STATIONARY;
		break;

	case MOVER_2TO3:
		VectorCopy(ent->pos2, ent->s.pos.trBase);
		VectorSubtract(ent->pos3, ent->pos2, delta);
		f = 1000.0 / ent->s.pos.trDuration;
		VectorScale(delta, f, ent->s.pos.trDelta);
		ent->s.pos.trType = TR_LINEAR_STOP;
		break;
	case MOVER_3TO2:
		VectorCopy(ent->pos3, ent->s.pos.trBase);
		VectorSubtract(ent->pos2, ent->pos3, delta);
		f = 1000.0 / ent->s.pos.trDuration;
		VectorScale(delta, f, ent->s.pos.trDelta);
		ent->s.pos.trType = TR_LINEAR_STOP;
		break;
	// END JOSEPH

	case MOVER_1TO2:        // opening
		VectorCopy(ent->pos1, ent->s.pos.trBase);
		VectorSubtract(ent->pos2, ent->pos1, delta);
//----(SA)	numerous changes start here
		ent->s.pos.trDuration = ent->gDuration;
		f                     = 1000.0 / ent->s.pos.trDuration;
		VectorScale(delta, f, ent->s.pos.trDelta);
		ent->s.pos.trType = TR_LINEAR_STOP;
		break;
	case MOVER_2TO1:        // closing
		VectorCopy(ent->pos2, ent->s.pos.trBase);
		VectorSubtract(ent->pos1, ent->pos2, delta);
		if (ent->closespeed)                            //----(SA)	handle doors with different close speeds
		{
			ent->s.pos.trDuration = ent->gDurationBack;
			f                     = 1000.0 / ent->gDurationBack;
		}
		else
		{
			ent->s.pos.trDuration = ent->gDuration;
			f                     = 1000.0 / ent->s.pos.trDuration;
		}
		VectorScale(delta, f, ent->s.pos.trDelta);
		ent->s.pos.trType = TR_LINEAR_STOP;
		break;


	case MOVER_POS1ROTATE:      // at close
		VectorCopy(ent->r.currentAngles, ent->s.apos.trBase);
		ent->s.apos.trType = TR_STATIONARY;
		break;
	case MOVER_POS2ROTATE:      // at open
		VectorCopy(ent->r.currentAngles, ent->s.apos.trBase);
		ent->s.apos.trType = TR_STATIONARY;
		break;
	case MOVER_1TO2ROTATE:      // opening
		VectorClear(ent->s.apos.trBase);                // set base to start position {0,0,0}

		if (kicked)
		{
			f                      = 2000.0 / ent->gDuration; // double speed when kicked open
			ent->s.apos.trDuration = ent->gDuration / 2.0;
		}
		else if (soft)
		{
			f                      = 500.0 / ent->gDuration; // 1/2 speed when soft opened
			ent->s.apos.trDuration = ent->gDuration * 2;
		}
		else
		{
			f = 1000.0 / ent->gDuration;
//				ent->s.apos.trDuration = ent->gDurationBack;	// (SA) durationback?
			ent->s.apos.trDuration = ent->gDuration;
		}
		VectorScale(ent->rotate, f * ent->angle, ent->s.apos.trDelta);
		ent->s.apos.trType = TR_LINEAR_STOP;
		if (ent->flags & FL_TOGGLE)
		{
			ent->active = qfalse;
		}
		break;
	case MOVER_2TO1ROTATE:      // closing
		VectorScale(ent->rotate, ent->angle, ent->s.apos.trBase);       // set base to end position
		// (kicked closes same as normally opened)
		// (soft closes at 1/2 speed)
		f                      = 1000.0 / ent->gDuration;
		ent->s.apos.trDuration = ent->gDuration;
		if (soft)
		{
			ent->s.apos.trDuration *= 2;
			f                      *= 0.5f;
		}
		VectorScale(ent->s.apos.trBase, -f, ent->s.apos.trDelta);
		ent->s.apos.trType = TR_LINEAR_STOP;
		ent->active        = qfalse;
		break;
	}
	BG_EvaluateTrajectory(&ent->s.pos, level.time, ent->r.currentOrigin, qfalse, ent->s.effect2Time);
//	if (!(ent->r.svFlags & SVF_NOCLIENT) || (ent->r.contents))	// RF, added this for bats, but this is safe for all movers, since if they aren't solid, and aren't visible to the client, they don't need to be linked
//		trap_LinkEntity( ent );

	if (!(ent->r.svFlags & SVF_NOCLIENT) || (ent->r.contents))      // RF, added this for bats, but this is safe for all movers, since if they aren't solid, and aren't visible to the client, they don't need to be linked
	{
		trap_LinkEntity(ent);
		if (strstr(ent->classname, "func_door") != ent->classname)
		{
			// if this entity is blocking AAS, then update it
			if (ent->AASblocking && ent->s.pos.trType == TR_STATIONARY)
			{
				// set new areas
				G_SetAASBlockingEntity(ent, AAS_AREA_DISABLED);
			}
		}
		else if (ent->key || ent->allowteams)       // doors should only block when locked and at POS1
		{
			switch (moverState)
			{
			case MOVER_POS1ROTATE:
			case MOVER_POS1:
				// block
				if (ent->allowteams)
				{
					// team specific
					int blockflags = 0;
					//
					if (ent->allowteams & ALLOW_AXIS_TEAM)
					{
						blockflags |= AAS_AREA_TEAM_AXIS;
						if (ent->allowteams & ALLOW_DISGUISED_CVOPS)
						{
							blockflags |= AAS_AREA_TEAM_ALLIES_DISGUISED;
						}
					}
					if (ent->allowteams & ALLOW_ALLIED_TEAM)
					{
						blockflags |= AAS_AREA_TEAM_ALLIES;
						if (ent->allowteams & ALLOW_DISGUISED_CVOPS)
						{
							blockflags |= AAS_AREA_TEAM_AXIS_DISGUISED;
						}
					}
					//
					if (!blockflags)
					{
						if (g_cheats.integer)
						{
							G_Printf("WARNING: %s at %s has allowteams flag, but no spawnflags to define which team is allowed\n", ent->classname, vtos(ent->r.absmin));
						}
					}
					else
					{
						G_SetAASBlockingEntity(ent, blockflags);
					}
				}
				else
				{
					// blocked for all
					G_SetAASBlockingEntity(ent, AAS_AREA_DISABLED);
				}
				break;
			default:
				// no blocking
				G_SetAASBlockingEntity(ent, AAS_AREA_ENABLED);
				break;
			}
		}
		else        // door is not locked, make sure it's not blocking
		{
			G_SetAASBlockingEntity(ent, AAS_AREA_ENABLED);
		}
	}


}
