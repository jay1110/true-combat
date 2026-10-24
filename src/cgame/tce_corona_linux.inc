/* TC:E Linux CG_Corona @ 00062bf6. Windows keeps tce_corona.inc. */
/* Linux uses the shared native normalization contract, including its float length. */
static float CG_TCECoronaNormalize(vec3_t v)
{
	return VectorNormalize(v);
}

static void CG_TCECoronaQuad(qhandle_t shader, const vec3_t top, const vec3_t bottom,
                             const vec3_t right, float radius, const vec3_t color)
{
	polyVert_t v[4];
	int        i, j;
	memset(v, 0, sizeof(v));
	for (i = 0; i < 4; i++)
	{
		const float *p = i < 2 ? top : bottom;
		VectorMA(p, (i == 0 || i == 3) ? radius : -radius, right, v[i].xyz);
		v[i].st[0] = (i == 0 || i == 3) ? 1 : 0;
		v[i].st[1] = i >= 2 ? 1 : 0;
		for (j = 0; j < 3; j++)
			v[i].modulate[j] = (byte)(int)((long double)color[j] * 255.f);
		v[i].modulate[3] = 255;
	}
	trap_R_AddPolyToScene(shader, 4, v);
}
static void CG_TCECoronaSprite(qhandle_t shader, const vec3_t origin, float radius, const vec3_t color,
                               float red, float green, float blue, float gain)
{
	refEntity_t re;
	memset(&re, 0, sizeof(re));
	re.reType = RT_SPRITE;
	re.customShader  = shader;
	re.radius = radius;
	VectorCopy(origin, re.origin);
	re.shaderRGBA[0] = (byte)(int)((long double)gain * color[0] * red);
	re.shaderRGBA[1] = (byte)(int)((long double)gain * color[1] * green);
	re.shaderRGBA[2] = (byte)(int)((long double)gain * color[2] * blue);
	re.shaderRGBA[3] = 255;
	trap_R_AddRefEntityToScene(&re);
}
static void CG_TCECoronaShake(vec3_t origin)
{
	CG_ApplyCameraShakeToVec(origin);
}
static void CG_Corona(centity_t *cent)
{
	entityState_t *es = &cent->currentState;
	vec3_t        point, dir, angles = { 0, 0, 0 }, rgb, color, top, bottom, origin, ray, temp;
	float         distance, dot, vertical = 1, attenuation = 1, size, visibility = 2, gain, nearGain;
	long double   fade = 1.0L;
	qboolean      sun = es->clientNum == 1, visible = qfalse, typeBright, typeWide;
	trace_t       tr;
	int           i, j, style, seed;
	qhandle_t     shader;
	if (es->clientNum == 2)
	{
		return;
	}
	for (i = 0; i < 3; i++)
		rgb[i] = ((es->dl_intensity >> (i * 8)) & 255) * (1.f / 255.f);
	if (sun)
	{
		angles[0] = -es->eventParm;
		angles[1] = es->frame;
		if (!cg.tceEyeFlareFrame)
		{
			AngleVectors(angles, cg.tceSunDirection, NULL, NULL);
			cg.tceEyeFlareFrame = 1;
		}
		VectorMA(cg.refdef_current->vieworg, 16384, cg.tceSunDirection, point);
		cg.tceCoronaBlendAlpha = 0;
	}
	else
	{
		VectorCopy(cent->lerpOrigin, point);
	}
	VectorSubtract(point, cg.refdef_current->vieworg, dir);
	if (sun)
	{
		CG_TCECoronaNormalize(dir);
	}
	distance = VectorNormalize2(dir, dir);
	dot      = DotProduct(dir, cg.refdef_current->viewaxis[0]);
	if (sun)
	{
		if (dot < 0)
		{
			return;
		}
	}
	else
	{
		if (dot >= .1f)
		{
			if (es->dmgFlags)
			{
				vertical = dir[2] < 0 ? 0 : dir[2];
			}
			if (es->effect2Time)
			{
				float cone = (float)(1.0L - fabsl((long double)dir[2]) * (long double)1.082);
				vertical = dir[2] < 0 ? 0 : dir[2];
				if (cone > 0)
				{
					if (cone > 1)
					{
						cone = 1;
					}
					gain = distance * .001953125f;
					if (gain < 1)
					{
						gain = 1;
					}
					gain = cone * cg.tceSparkIntensity * (1.f / gain);
					for (i = 0; i < 3; i++)
						color[i] = (float)(((es->dl_intensity >> (i * 8)) & 255) * (long double)(1.f / 255.f) * gain * .5f);
					style = es->effect1Time;
					if (style < 1 || style > 3)
					{
						style = 0;
					}
					shader = es->modelindex ? cgs.gameShaders[es->modelindex] : cgs.media.tceCoronaCone[style];
					VectorCopy(point, top);
					VectorCopy(point, bottom);
					size    = (float)es->frame * .015625f * (float)es->effect2Time;
					top[2] += size * .125f;
					bottom[2] -= size * 1.875f;
					CG_TCECoronaQuad(shader, top, bottom, cg.refdef.viewaxis[1], (float)es->effect2Time, color);
				}
			}
		}
		if (dot <= .4 || vertical == 0)
		{
			return;
		}
	}
	CG_Trace(&tr, cg.refdef_current->vieworg, NULL, NULL, point, -1, MASK_SOLID | CONTENTS_BODY);
	if (sun)
	{
		if (tr.surfaceFlags & SURF_SKY)
		{
			visibility = 3;
		}
		for (j = 0; j < 4; j++)
		{
			VectorCopy(angles, temp);
			temp[j < 2 ? 0 : 1] += (j & 1) ? -1 : 1;
			AngleVectors(temp, ray, NULL, NULL);
			VectorMA(cg.refdef_current->vieworg, 16384, ray, origin);
			CG_Trace(&tr, cg.refdef_current->vieworg, NULL, NULL, origin, -1, MASK_SOLID | CONTENTS_BODY);
			if (tr.surfaceFlags & SURF_SKY)
			{
				visibility += 1;
			}
		}
		visibility               *= .1428571492433548f;
		cent->tceCoronaVisibility = (visibility * .1f + cent->tceCoronaVisibility) * .9090908765792847f;
		if (visibility > .3f)
		{
			visible = qtrue;
		}
	}
	else if (tr.fraction == 1)
	{
		visible = qtrue;
	}
	if (visible)
	{
		cent->tceCoronaLastVisible = cg.time;
	}
	style = es->otherEntityNum2;
	typeBright = style == 1 || style == 2;
	typeWide = !typeBright;
	if (style < 1 || style > 3)
	{
		style = 0;
	}
	shader = cgs.media.tceCoronaFlare[style];
	size   = es->density * (1.f / 255.f);
	if (sun)
	{
		size *= 6;
	}
	if (!visible && cg.time - cent->tceCoronaLastVisible > 499)
	{
		cent->miscTime = cg.time;
		return;
	}
	if (!sun)
	{
		nearGain = distance * .0625f;
		if (nearGain < 1)
		{
			nearGain = 1;
		}
		nearGain = (1.f / nearGain) * 7.f;
		if (distance < 256)
		{
			float forward = (dot - .4f) * 1.6666666269302368f;
			float closeGain;
			closeGain = (256.f - distance) * .004166666883975267f;
			if (closeGain > 1)
			{
				closeGain = 1;
			}
			if (forward > 0)
			{
				nearGain = closeGain * closeGain * 7.f * forward + nearGain;
			}
		}
		attenuation = distance * .001953125f;
		if (attenuation < 1)
		{
			attenuation = 1;
		}
		attenuation = 1 / attenuation;
		size       *= nearGain + (typeWide ? attenuation * .5f + .5f : attenuation);
	}
	if (!visible)
	{
		fade = 1.0L - (1.0L / 500.f) * (cg.time - cent->tceCoronaLastVisible);
	}
	else if (cg.time - cent->miscTime < 50)
	{
		fade = (cg.time - cent->miscTime) * (1.0L / 50.f);
	}
	for (i = 0; i < 3; i++)
	{
		color[i] = (float)(((es->dl_intensity >> (i * 8)) & 255) * (long double)(1.f / 255.f) * fade);
		if (typeBright || typeWide)
		{
			color[i] *= attenuation;
		}
		if (vertical < 1)
		{
			color[i] *= vertical;
		}
		color[i] *= sun ? cent->tceCoronaVisibility : cg.tceSparkIntensity;
	}
	if (sun)
	{
		float sunGain = (dot - .8f) * 5.f;
		if (sunGain < 0)
		{
			sunGain = 0;
		}
		sunGain *= sunGain;
		size = (sunGain * sunGain * (float)es->effect3Time * .1f + 1.f + sunGain) * size;
		if (es->effect3Time)
		{
			float alpha = (float)((long double)es->effect3Time * fade * cent->tceCoronaVisibility * sunGain * (1.f / 255.f));
			if (alpha > 0)
			{
				cg.tceCoronaBlendAlpha = alpha > 1 ? 1 : alpha;
				VectorCopy(color, cg.tceCoronaBlendColor);
			}
		}
	}
	if (cg.tceEyeSampleTime != cg.clientFrame)
	{
		cg.tceEyeSampleTime = cg.clientFrame;
		cg.tceEyeFlare = 0;
	}
	{
		float eyeGain = dot;
		if (!sun && distance > 96)
		{
			eyeGain = (1.f / distance) * 96.f * eyeGain;
		}
		if (eyeGain > 0)
		{
			cg.tceEyeFlare = (color[1] + color[0] + color[2]) * size *
			                 eyeGain * eyeGain * .3333333432674408f + cg.tceEyeFlare;
		}
	}
	VectorCopy(cg.refdef.vieworg, origin);
	CG_TCECoronaShake(origin);
	VectorSubtract(point, origin, ray);
	CG_TCECoronaNormalize(ray);
	VectorMA(origin, 8, ray, temp);
	VectorMA(temp, size, cg.refdef.viewaxis[2], bottom);
	VectorMA(bottom, -2 * size, cg.refdef.viewaxis[2], top);
	CG_TCECoronaQuad(shader, top, bottom, cg.refdef.viewaxis[1], size, color);
	if (!visible)
	{
		cent->miscTime = cg.time;
		return;
	}
	if (sun || (cg.tceAimActive && cg.tceAimComplete && !cg.renderingThirdPerson && es->otherEntityNum2 != 1 && weaponDef[cg.snap->ps.weapon].scoped > 0))
	{
		float lensGain;
		gain = DotProduct(cg.refdef.viewaxis[0], dir) - .975f;
		if (gain <= 0)
		{
			return;
		}
		gain = (float)es->density * gain * .062745101749897f;
		lensGain = gain;
		for (i = 0; i < 3; i++)
			color[i] = rgb[i] * (sun ? cent->tceCoronaVisibility : 1.f);
		VectorAdd(cg.refdef.viewaxis[0], dir, ray);
		CG_TCECoronaNormalize(ray);
		VectorMA(cg.refdef.vieworg, 8, ray, temp);
		CG_TCECoronaSprite(cgs.media.tceLensFlare[0], temp, .5f, color, 178.5f, 102, 255, lensGain);
		gain = 2 * DotProduct(cg.refdef.viewaxis[0], ray);
		for (i = 0; i < 3; i++)
			ray[i] = cg.refdef.viewaxis[0][i] * gain - ray[i];
		VectorMA(cg.refdef.vieworg, 8, ray, temp);
		CG_TCECoronaSprite(cgs.media.tceLensFlare[1], temp, .7f, color, 255, 127.5f, 204, lensGain);
		VectorAdd(cg.refdef.viewaxis[0], ray, ray);
		CG_TCECoronaNormalize(ray);
		VectorMA(cg.refdef.vieworg, 8, ray, temp);
		CG_TCECoronaSprite(cgs.media.tceLensFlare[1], temp, .35f, color, 204, 255, 127.5f, lensGain);
	}
	if (es->modelindex2 && !sun)
	{
		float t = (float)(((long double)cg.time * .001f * 2.0L) * (long double)3.141592653589793);
		float phase, r0, r1, r2, r3, radius;
		seed = es->number;
		phase = Q_crandom(&seed) * 5;
		for (j = 0; j < es->modelindex2; j++)
		{
			int randomSeed = j + 1 + seed;
			r0      = Q_crandom(&randomSeed);
			randomSeed++;
			r1 = Q_crandom(&randomSeed);
			randomSeed++;
			r2 = Q_crandom(&randomSeed);
			randomSeed++;
			r3 = Q_crandom(&randomSeed);
			{
				long double xPhase, yPhase, zPhase;
				float xOffset, yOffset;

				xPhase = j * (long double)1.4925373f +
				         ((long double)r3 * .5f + 1.f) * t * .25f + 3.978f + phase;
				yPhase = ((long double)r1 * .5f + 1.f) * t * .25f +
				         1.374f - j * (long double)1.1235955f + phase;
				zPhase = ((long double)r0 * .5f + 1.f) * t * .25f +
				         7.457f + j * (long double).9009009f + phase;
				xOffset = (float)((r1 * 8.0L + 13) * sinl(xPhase));
				yOffset = (float)((r0 * 8.0L + 13) * sinl(yPhase));
				temp[0] = point[0] + xOffset;
				temp[1] = point[1] + yOffset;
				temp[2] = (float)((long double)(point[2] - (float)(j + 8)) +
				                  (r2 * 8.0L + 13) * sinl(zPhase));
			}
			radius  = (float)(r1 * .1 + .6000000238418579);
			VectorMA(temp, -radius, cg.refdef.viewaxis[2], top);
			VectorMA(temp, radius, cg.refdef.viewaxis[2], bottom);
			CG_TCECoronaQuad(cgs.media.tceLensFlare[1], top, bottom, cg.refdef.viewaxis[1], radius, rgb);
		}
	}
}
