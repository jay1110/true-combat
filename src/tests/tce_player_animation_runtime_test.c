/* Actual original/port DLL controller comparison; no engine or function hooks. */
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include "../cgame/tce_weapon_media.h"
#include <windows.h>
#include <stdio.h>

typedef void (__cdecl *rate_fn)(void *, lerpFrame_t *, int, void *, int);
typedef void (__cdecl *frame_fn)(void *, void *, lerpFrame_t *, int, float);
static unsigned random_state = 0x1731f00du;
static int phase, running_case;
static byte *original_base, *port_base;
static LONG WINAPI fault_trace(EXCEPTION_POINTERS *event)
{
	if (event->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
		fprintf(stderr, "animation harness fault case=%d phase=%d eip=%p original-relative=%lx port-relative=%lx\n",
		        running_case, phase, event->ExceptionRecord->ExceptionAddress,
		        (unsigned long)((uintptr_t)event->ExceptionRecord->ExceptionAddress - (uintptr_t)original_base),
		        (unsigned long)((uintptr_t)event->ExceptionRecord->ExceptionAddress - (uintptr_t)port_base));
	return EXCEPTION_CONTINUE_SEARCH;
}
static unsigned next_random(void) { random_state = random_state * 1664525u + 1013904223u; return random_state; }

static char file_payload[20000];
static int file_length, unexpected_traps;
static const char *token_cursor;
static int model_count;
static int next_fixture_token(pc_token_t *token)
{
	const char *p = token_cursor;
	int length = 0;
	memset(token, 0, sizeof(*token));
	for (;;)
	{
		while (*p && (unsigned char)*p <= 32) ++p;
		if (p[0] == '/' && p[1] == '/') { while (*p && *p != '\n') ++p; continue; }
		if (p[0] == '/' && p[1] == '*') { p += 2; while (*p && !(p[0] == '*' && p[1] == '/')) ++p; if (*p) p += 2; continue; }
		break;
	}
	if (!*p) { token_cursor = p; return 0; }
	if (*p == '"')
	{
		token->type = TT_STRING; ++p;
		while (*p && *p != '"' && length < MAX_TOKENLENGTH - 1) token->string[length++] = *p++;
		if (*p == '"') ++p;
	}
	else if (*p == '{' || *p == '}' || *p == '-')
	{
		token->type = TT_PUNCTUATION; token->string[length++] = *p++;
	}
	else
	{
		while (*p && (unsigned char)*p > 32 && *p != '{' && *p != '}' && length < MAX_TOKENLENGTH - 1) token->string[length++] = *p++;
		token->type = token->string[0] >= '0' && token->string[0] <= '9' ? TT_NUMBER : TT_NAME;
	}
	token->string[length] = 0;
	token->intvalue = atoi(token->string); token->floatvalue = (float)atof(token->string);
	token_cursor = p;
	return 1;
}
static intptr_t QDECL fixture_syscall(intptr_t operation, ...)
{
	va_list args;
	intptr_t result = 0;
	va_start(args, operation);
	switch (operation)
	{
	case CG_FS_FOPENFILE:
		(void)va_arg(args, const char *);
		*va_arg(args, int *) = 7;
		result = file_length;
		break;
	case CG_FS_READ:
	{
		void *buffer = va_arg(args, void *);
		int length = va_arg(args, int);
		if (length < 0 || length > file_length) ++unexpected_traps;
		else memcpy(buffer, file_payload, length);
		break;
	}
	case CG_FS_FCLOSEFILE: break;
	case CG_MILLISECONDS: result = 0; break;
	case CG_PC_LOAD_SOURCE: token_cursor = file_payload; model_count = 0; result = 1; break;
	case CG_PC_READ_TOKEN:
		(void)va_arg(args, int); result = next_fixture_token(va_arg(args, pc_token_t *)); break;
	case CG_PC_FREE_SOURCE: break;
	case CG_PC_SOURCE_FILE_AND_LINE:
		(void)va_arg(args, int); strcpy(va_arg(args, char *), "animation-fixture"); *va_arg(args, int *) = 1; break;
	case CG_R_REGISTERMODEL: result = ++model_count; break;
	case CG_PRINT: break;
	default: ++unexpected_traps; fprintf(stderr, "unexpected fixture syscall %d\n", (int)operation); break;
	}
	va_end(args);
	return result;
}

static void *mapped(HMODULE dll, const char *map, const char *symbol)
{
	FILE *file = fopen(map, "rb");
	char line[2048], name[256];
	unsigned address, preferred = 0;
	if (!file) return NULL;
	while (fgets(line, sizeof(line), file))
	{
		if (sscanf(line, " Preferred load address is %x", &address) == 1)
		{
			preferred = address;
			continue;
		}
		if (sscanf(line, " %*s %255s %x", name, &address) == 2 && !strcmp(name, symbol))
		{
			fclose(file);
			return preferred && address >= preferred ? (byte *)dll + (address - preferred) : NULL;
		}
	}
	fclose(file);
	return NULL;
}

int main(int argc, char **argv)
{
	HMODULE original, port;
	byte *base;
	cg_t *port_cg;
	vmCvar_t *port_speed, *port_debug;
	rate_fn original_rate, port_rate;
	frame_fn original_frame, port_frame;
	static centity_t source_cent;
	static byte raw_cent[0xa94], raw_snap[0x500];
	static snapshot_t source_snap;
	static animation_t animations[2];
	int c, i, rate_cases = 0, frame_cases = 0, frame_failures = 0, speed_differences = 0, interpolation_differences = 0;
	int generic_interpolation_differences = 0;
	if (argc < 4 || sizeof(void *) != 4 || sizeof(animation_t) != 112 || sizeof(lerpFrame_t) != 84)
	{
		fprintf(stderr, "usage: original-cgame.dll port-cgame.dll port-symbols.map (Win32 only)\n"); return 2;
	}
	AddVectoredExceptionHandler(1, fault_trace);
	original = LoadLibraryA(argv[1]); port = LoadLibraryA(argv[2]);
	if (!original || !port) { fprintf(stderr, "LoadLibrary failed: %lu\n", GetLastError()); return 2; }
	base = (byte *)original;
	original_base = base; port_base = (byte *)port;
	((void (__cdecl *)(intptr_t (QDECL *)(intptr_t, ...)))GetProcAddress(original, "dllEntry"))(fixture_syscall);
	((void (__cdecl *)(intptr_t (QDECL *)(intptr_t, ...)))GetProcAddress(port, "dllEntry"))(fixture_syscall);
	port_cg = (cg_t *)mapped(port, argv[3], "_cg");
	port_speed = (vmCvar_t *)mapped(port, argv[3], "_cg_animSpeed");
	port_debug = (vmCvar_t *)mapped(port, argv[3], "_cg_debugAnim");
	port_rate = (rate_fn)mapped(port, argv[3], "_CG_RunLerpFrameRate");
	port_frame = (frame_fn)mapped(port, argv[3], "_CG_RunLerpFrame");
	if (!port_cg || !port_speed || !port_debug || !port_rate || !port_frame)
	{
		fprintf(stderr, "Required current port map symbols missing\n"); return 2;
	}
	original_rate = (rate_fn)(base + 0x53910);
	original_frame = (frame_fn)(base + 0x53550);
	port_speed->integer = *(int *)(base + 0x40120ec) = 1;
	port_debug->integer = *(int *)(base + 0x4034f4c) = 0;
	for (c = 0; c < 60000; ++c)
	{
		running_case = c;
		lerpFrame_t initial, *source, *expected;
		int torso = c & 1, time = 10000 + (int)(next_random() % 10000), elapsed = 1 + (int)(next_random() % 180);
		int origin_time = time - elapsed, which = (c >> 1) & 1, mode = (c >> 2) & 7;
		float manual_speed = (float)(1 + next_random() % 8) / 4.0f;
		memset(&source_cent, 0, sizeof(source_cent)); memset(raw_cent, 0, sizeof(raw_cent));
		memset(&initial, 0, sizeof(initial)); memset(animations, 0, sizeof(animations));
		for (i = 0; i < 2; ++i)
		{
			animations[i].mdxFile = 101 + i; animations[i].firstFrame = 40 + i * 40;
			animations[i].numFrames = 1 + next_random() % 25; animations[i].frameLerp = 1 + next_random() % 120;
			animations[i].loopFrames = c % 3 ? animations[i].numFrames : 0;
			animations[i].initialLerp = animations[i].frameLerp;
			animations[i].moveSpeed = c % 4 ? 10 + next_random() % 150 : 0;
			animations[i].flags = mode & 7;
		}
		initial.animation = &animations[0]; initial.animationNumber = initial.oldAnimationNumber = 0;
		initial.frame = initial.oldFrame = animations[0].firstFrame + next_random() % animations[0].numFrames;
		initial.frameModel = initial.oldFrameModel = animations[0].mdxFile;
		initial.frameTime = time + (int)(next_random() % 300) - 150;
		initial.oldFrameTime = origin_time; initial.animationTime = time - 200;
		initial.animSpeedScale = 1.0f; initial.oldFrameSnapshotTime = c % 7 ? time - 50 : 0;
		initial.oldFramePos[0] = (float)(next_random() % 100);
		initial.oldFramePos[1] = (float)(next_random() % 100);
		initial.oldFramePos[2] = (float)(next_random() % 100);
		source_cent.pe.legs = source_cent.pe.torso = initial;
		source_cent.pe.legs.animation = &animations[which]; source_cent.pe.legs.animationNumber = which;
		source_cent.pe.torso.animation = &animations[which]; source_cent.pe.torso.animationNumber = which;
		source = torso ? &source_cent.pe.torso : &source_cent.pe.legs;
		*source = initial;
		memcpy(raw_cent + 0x268, &source_cent.pe.legs, sizeof(initial));
		memcpy(raw_cent + 0x2bc, &source_cent.pe.torso, sizeof(initial));
		expected = (lerpFrame_t *)(raw_cent + (torso ? 0x2bc : 0x268));
		source_cent.pe.animSpeed = *(float *)(raw_cent + 0x6ec) = manual_speed;
		source_cent.currentState.number = *(int *)raw_cent = c % 2;
		source_cent.currentState.eType = *(int *)(raw_cent + 4) = c % 11 ? ET_PLAYER : ET_CORPSE;
		for (i = 0; i < 3; ++i)
		{
			source_cent.lerpOrigin[i] = *(float *)(raw_cent + 0x710 + i * 4) = (float)(next_random() % 100);
			source_cent.currentState.pos.trBase[i] = *(float *)(raw_cent + 0x18 + i * 4) = source_cent.lerpOrigin[i];
		}
		port_cg->time = *(int *)(base + 0x407dea4) = time;
		port_cg->latestSnapshotTime = *(int *)(base + 0x40352c0) = time;
		source_snap.ps.clientNum = *(int *)(raw_snap + 0xcc) = 0;
		port_cg->snap = port_cg->nextSnap = &source_snap;
		*(void **)(base + 0x40352c4) = *(void **)(base + 0x40352c8) = raw_snap;
		phase = 1; original_rate(NULL, expected, 0, raw_cent, 0);
		phase = 2; port_rate(NULL, source, 0, &source_cent, 0);
		++rate_cases;
		if (expected->animSpeedScale != source->animSpeedScale) ++speed_differences;
		if (expected->backlerp != source->backlerp) ++interpolation_differences;
		if (expected->frame != source->frame || expected->oldFrame != source->oldFrame ||
		    expected->frameTime != source->frameTime || expected->oldFrameTime != source->oldFrameTime ||
		    expected->frameModel != source->frameModel || expected->oldFrameModel != source->oldFrameModel)
		{
			if (frame_failures++ < 8)
			{
				unsigned old_bits, new_bits;
				memcpy(&old_bits, &expected->animSpeedScale, 4); memcpy(&new_bits, &source->animSpeedScale, 4);
				printf("RATE DIFFERENCE case=%d torso=%d mode=%d move=%d time=%d oldTime=%d oldFrame=%d initialFrameTime=%d speed=%g/%g bits=%08x/%08x frame=%d/%d frameTime=%d/%d frameLerp=%d manual=%g oldPos=%g,%g,%g origin=%g,%g,%g\n", c, torso, mode, animations[0].moveSpeed, time, origin_time, initial.frame, initial.frameTime, expected->animSpeedScale, source->animSpeedScale, old_bits, new_bits, expected->frame, source->frame, expected->frameTime, source->frameTime, animations[0].frameLerp, manual_speed, initial.oldFramePos[0], initial.oldFramePos[1], initial.oldFramePos[2], source_cent.lerpOrigin[0], source_cent.lerpOrigin[1], source_cent.lerpOrigin[2]);
			}
		}
		/* Unhooked ordinary non-switching generic controller as integrated in each DLL. */
		*source = initial; *expected = initial;
		phase = 3; original_frame(raw_cent, NULL, expected, 0, manual_speed);
		phase = 4; port_frame(&source_cent, NULL, source, 0, manual_speed);
		++frame_cases;
		if (expected->backlerp != source->backlerp) ++generic_interpolation_differences;
		if (expected->frame != source->frame || expected->oldFrame != source->oldFrame || expected->frameTime != source->frameTime)
		{
			if (frame_failures++ < 8) printf("FRAME DIFFERENCE case=%d frames=%d/%d times=%d/%d\n", c, expected->frame, source->frame, expected->frameTime, source->frameTime);
		}
	}
	printf("actual DLL animation controllers: rate=%d generic=%d frame_or_time_differences=%d speed_float_differences=%d backlerp_float_differences=%d generic_backlerp_float_differences=%d\n", rate_cases, frame_cases, frame_failures, speed_differences, interpolation_differences, generic_interpolation_differences);
	{
		int (__cdecl *original_config)(const char *, tce_weaponInfo_t *) = (void *)(base + 0x6e870);
		int (__cdecl *port_config)(const char *, tce_weaponInfo_t *) = mapped(port, argv[3], "_TCE_CG_ParseWeaponConfig");
		int parsed = 0, failed = 0;
		if (!port_config) return 2;
		for (i = 4; i < argc; ++i)
		{
			FILE *fixture = fopen(argv[i], "rb");
			tce_weaponInfo_t expected, actual;
			int a, b;
			if (!fixture) return 2;
			file_length = (int)fread(file_payload, 1, sizeof(file_payload) - 1, fixture);
			fclose(fixture); file_payload[file_length] = 0;
			if (strstr(argv[i], ".anim"))
			{
				static animModelInfo_t original_group, port_group;
				void (__cdecl *original_clear)(void) = (void *)(base + 0x2a60);
				void (__cdecl *port_clear)(void) = mapped(port, argv[3], "_BG_ClearAnimationPool");
				int (__cdecl *original_group_parse)(const char *, animModelInfo_t *) = (void *)(base + 0x2a80);
				int (__cdecl *port_group_parse)(const char *, animModelInfo_t *) = mapped(port, argv[3], "_BG_R_RegisterAnimationGroup");
				int differences = 0;
				memset(&original_group, 0, sizeof(original_group)); memset(&port_group, 0, sizeof(port_group));
				original_clear(); port_clear();
				phase = 7; a = original_group_parse(argv[i], &original_group);
				phase = 8; b = port_group_parse(argv[i], &port_group);
				if (a != b || original_group.numAnimations != port_group.numAnimations) ++differences;
				for (c = 0; c < original_group.numAnimations && c < port_group.numAnimations; ++c)
					if (memcmp(original_group.animations[c], port_group.animations[c], sizeof(animation_t)))
					{
						++differences;
						printf("BODY ANIM DIFFERENCE %d %s frameLerp=%d/%d\n", c, original_group.animations[c]->name, original_group.animations[c]->frameLerp, port_group.animations[c]->frameLerp);
					}
				printf("actual body animgroup parser: animations=%d/%d differences=%d returns=%d/%d\n", original_group.numAnimations, port_group.numAnimations, differences, a, b);
				{
					static bg_character_t original_character, source_character;
					static centity_t player;
					static byte raw_player[0xa94];
					cgs_t *port_cgs = mapped(port, argv[3], "_cgs");
					vmCvar_t *no_anims = mapped(port, argv[3], "_cg_noPlayerAnims");
					void (__cdecl *old_player)(void *, refEntity_t *) = (void *)(base + 0x550e0);
					void (__cdecl *new_player)(centity_t *, refEntity_t *) = mapped(port, argv[3], "_CG_PlayerAnimation");
					int index, step, player_cases = 0, player_differences = 0;
					printf("player fixture layout: client=%u character-offset=%u character-model-offset=%u\n", (unsigned)sizeof(clientInfo_t), (unsigned)offsetof(clientInfo_t, character), (unsigned)offsetof(bg_character_t, animModelInfo));
					if (!port_cgs || !no_anims || !new_player || offsetof(clientInfo_t, character) != 0x16c || offsetof(bg_character_t, animModelInfo) != 0x83c) return 2;
					original_character.animModelInfo = &original_group;
					source_character.animModelInfo = &port_group;
					*(void **)(base + 0x2580a10 + 0x16c) = &original_character;
					port_cgs->clientinfo[0].character = &source_character;
					no_anims->integer = *(int *)(base + 0x4849f6c) = 0;
					for (index = 0; index < original_group.numAnimations; ++index)
					{
						memset(&player, 0, sizeof(player)); memset(raw_player, 0, sizeof(raw_player));
						player.currentState.eType = *(int *)(raw_player + 4) = ET_PLAYER;
						player.pe.animSpeed = *(float *)(raw_player + 0x6ec) = 1.0f;
						player.currentState.legsAnim = *(int *)(raw_player + 0xec) = index | (index & 1 ? ANIM_TOGGLEBIT : 0);
						player.currentState.torsoAnim = *(int *)(raw_player + 0xf0) = (index + 37) % original_group.numAnimations;
						for (step = 0; step < 12; ++step)
						{
							refEntity_t expected_body, actual_body;
							memset(&expected_body, 0, sizeof(expected_body)); memset(&actual_body, 0, sizeof(actual_body));
							port_cg->time = *(int *)(base + 0x407dea4) = 10000 + step * 50;
							port_cg->latestSnapshotTime = *(int *)(base + 0x40352c0) = port_cg->time;
							phase = 9; old_player(raw_player, &expected_body);
							phase = 10; new_player(&player, &actual_body);
							++player_cases;
							if (expected_body.frame != actual_body.frame || expected_body.oldframe != actual_body.oldframe || expected_body.torsoFrame != actual_body.torsoFrame || expected_body.oldTorsoFrame != actual_body.oldTorsoFrame || expected_body.frameModel != actual_body.frameModel || expected_body.oldframeModel != actual_body.oldframeModel || expected_body.torsoFrameModel != actual_body.torsoFrameModel || expected_body.oldTorsoFrameModel != actual_body.oldTorsoFrameModel)
							{
								if (player_differences++ < 8) printf("PLAYER BODY DIFFERENCE anim=%d step=%d legs=%d/%d torso=%d/%d\n", index, step, expected_body.frame, actual_body.frame, expected_body.torsoFrame, actual_body.torsoFrame);
							}
						}
						}
						printf("actual CG_PlayerAnimation/refEntity integration: cases=%d frame_model_differences=%d\n", player_cases, player_differences);
						differences += player_differences;
				}
				failed += differences;
				continue;
			}
			memset(&expected, 0, sizeof(expected)); memset(&actual, 0, sizeof(actual));
			phase = 5; a = original_config(argv[i], &expected);
			phase = 6; b = port_config(argv[i], &actual);
			++parsed;
			if (a != b || memcmp(&actual, &expected, sizeof(actual)))
			{
				++failed; printf("CONFIG DIFFERENCE %s returns=%d/%d\n", argv[i], a, b);
				for (c = 0; c < 13; ++c)
					if (memcmp(&actual.animations[c], &expected.animations[c], sizeof(animation_t)))
						printf(" anim=%d first=%d/%d count=%d/%d fps-ms=%d/%d loop=%d/%d flags=%x/%x\n", c, expected.animations[c].firstFrame, actual.animations[c].firstFrame, expected.animations[c].numFrames, actual.animations[c].numFrames, expected.animations[c].frameLerp, actual.animations[c].frameLerp, expected.animations[c].loopFrames, actual.animations[c].loopFrames, expected.animations[c].moveSpeed, actual.animations[c].moveSpeed);
			}
		}
		printf("actual PK3 weapon CFG parsers: files=%d complete_media_differences=%d unexpected_engine_traps=%d\n", parsed, failed, unexpected_traps);
		frame_failures += failed + unexpected_traps;
	}
	return frame_failures || speed_differences || interpolation_differences || generic_interpolation_differences ? 1 : 0;
}
