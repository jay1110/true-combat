#include "q_shared.h"
#include "bg_public.h"

#define MAX_SCRIPTSPEAKERS 256

static bg_speaker_t	scriptSpeakers[MAX_SCRIPTSPEAKERS];
static int			numScriptSpeakers;

void BG_ClearScriptSpeakerPool( void )
{
	numScriptSpeakers = 0;
}

int BG_NumScriptSpeakers( void )
{
	return numScriptSpeakers;
}

int BG_GetIndexForSpeaker(bg_speaker_t *speaker) {
#if !defined(Q3_VM) && (defined(_M_IX86) || defined(__i386__)) && \
    (defined(_WIN32) || defined(__linux__))
    typedef char speakerIndexLayout[(sizeof(bg_speaker_t) == 152 &&
        sizeof(unsigned) == 4 && sizeof(int) == 4 && sizeof(uintptr_t) == 4) ? 1 : -1];
    unsigned difference = (unsigned)(uintptr_t)speaker -
                          (unsigned)(uintptr_t)scriptSpeakers;
    int signedDifference;
    memcpy(&signedDifference, &difference, sizeof(signedDifference));
#if defined(_WIN32)
    return signedDifference / 152;
#else
    /* The Linux pointer-difference optimization first floors by eight,
     * then divides the remaining exact factor nineteen modulo 2^32.
     * Keep that result for addresses that are not record boundaries too. */
    int groups = signedDifference / 8;
    unsigned indexBits;
    int index;
    if (signedDifference < 0 && signedDifference % 8 != 0) {
        --groups;
    }
    /* 19 * 0x286bca1b == 1 modulo 2^32. */
    indexBits = (unsigned)groups * 0x286bca1bu;
    memcpy(&index, &indexBits, sizeof(index));
    return index;
#endif
#else
    return speaker - scriptSpeakers;
#endif
}

bg_speaker_t *BG_GetScriptSpeaker( int index )
{
	if( index < 0 || index >= numScriptSpeakers ) {
		return NULL;
	}

	return &scriptSpeakers[ index ];
}

qboolean BG_SS_DeleteSpeaker(int index) {
    size_t offset, bytes;
    unsigned char *pool = (unsigned char *)scriptSpeakers;
    unsigned char *destination;
    const unsigned char *source;

    if (index < 0 || index >= numScriptSpeakers) {
        return qfalse;
    }

    destination = pool + (size_t)index * sizeof(bg_speaker_t);
    source = destination + sizeof(bg_speaker_t);
    bytes = sizeof(bg_speaker_t) * (numScriptSpeakers - index - 1);
    /* Native copies towards the lower address, a word at a time. Access
     * through the whole pool's byte representation permits crossing record
     * boundaries without an overlapping memcpy of the entire tail. */
    for (offset = 0; offset < bytes; offset += sizeof(unsigned)) {
        unsigned word;
        memcpy(&word, source + offset, sizeof(word));
        memcpy(destination + offset, &word, sizeof(word));
    }
    --numScriptSpeakers;
    return qtrue;
}

/* The 32-bit original record is 38 words. Keep wider SDK layouts intact. */
#if defined(_MSC_VER) && defined(_M_IX86)
typedef char speaker835Layout[(sizeof(bg_speaker_t)==152 && MAX_SCRIPTSPEAKERS==256) ? 1 : -1];
#endif
typedef char speakerCopyWordLayout[(sizeof(unsigned)==4 &&
    sizeof(bg_speaker_t)%sizeof(unsigned)==0) ? 1 : -1];

qboolean BG_SS_StoreSpeaker(bg_speaker_t *speaker) {
    bg_speaker_t *destination;
    size_t offset;

    if (numScriptSpeakers >= MAX_SCRIPTSPEAKERS) {
        return qfalse;
    }

    destination = &scriptSpeakers[numScriptSpeakers];
#if defined(_WIN32) || !defined(__linux__)
    /* Windows publishes the new count before copying the record. */
    ++numScriptSpeakers;
#endif
    /* One word at a time preserves the native forward-copy order even when
     * source and destination overlap. A whole-record memcpy would not. */
    for (offset = 0; offset < sizeof(*speaker); offset += sizeof(unsigned)) {
        unsigned word;
        memcpy(&word, (const unsigned char *)speaker + offset, sizeof(word));
        memcpy((unsigned char *)destination + offset, &word, sizeof(word));
    }
#if defined(__linux__) && !defined(_WIN32)
    /* Linux REP MOVSD completes before the pool count is incremented. */
    ++numScriptSpeakers;
#endif
    return qtrue;
}

static qboolean BG_SS_ParseError( int handle, char *format, ... )
{
	int line;
	char filename[128];
	va_list argptr;
	static char string[4096];

	va_start( argptr, format );
	Q_vsnprintf( string, sizeof(string), format, argptr );
	va_end( argptr );

	filename[0] = '\0';
	line = 0;
	trap_PC_SourceFileAndLine( handle, filename, &line );

	Com_Printf( S_COLOR_RED "ERROR: %s, line %d: %s\n", filename, line, string );

	trap_PC_FreeSource( handle );

	return qfalse;
}

static qboolean BG_SS_ParseSpeaker( int handle )
{
	pc_token_t		token;
	bg_speaker_t	speaker;

	memset( &speaker, 0, sizeof(speaker) );

	speaker.volume = 127;
	speaker.range = 1250;

	if( !trap_PC_ReadToken( handle, &token ) || Q_stricmp( token.string, "{" ) ) {
		return BG_SS_ParseError( handle, "expected '{'" );
	}

	while( 1 ) {
		if( !trap_PC_ReadToken( handle, &token ) ) {
			break;
		}

		if( token.string[0] == '}' ) {
			break;
		}

		if( !Q_stricmp( token.string, "noise" ) ) {
			if( !PC_String_ParseNoAlloc( handle, speaker.filename, sizeof(speaker.filename) ) ) {
				return BG_SS_ParseError( handle, "expected sound filename" );
			}
		} else if( !Q_stricmp( token.string, "origin" ) ) {
			if( !PC_Vec_Parse( handle, &speaker.origin ) ) {
				return BG_SS_ParseError( handle, "expected origin vector" );
			}
		} else if( !Q_stricmp( token.string, "targetname" ) ) {
			if( !PC_String_ParseNoAlloc( handle, speaker.targetname, sizeof(speaker.targetname) ) ) {
				return BG_SS_ParseError( handle, "expected targetname string" );
			} else {
				speaker.targetnamehash = BG_StringHashValue( speaker.targetname );
			}
		} else if( !Q_stricmp( token.string, "looped" ) ) {
			if( !trap_PC_ReadToken( handle, &token ) ) {
				return BG_SS_ParseError( handle, "expected loop value" );
			} else {
				if( !Q_stricmp( token.string, "no" ) ) {
					speaker.loop = S_LT_NOT_LOOPED;
				} else if( !Q_stricmp( token.string, "on" ) ) {
					speaker.loop = S_LT_LOOPED_ON;
					speaker.activated = qtrue;
				} else if( !Q_stricmp( token.string, "off" ) ) {
					speaker.loop = S_LT_LOOPED_OFF;
				} else {
					return BG_SS_ParseError( handle, "unknown loop value '%s'", token.string );
				}
			}
		} else if( !Q_stricmp( token.string, "broadcast" ) ) {
			if( !trap_PC_ReadToken( handle, &token ) ) {
				return BG_SS_ParseError( handle, "expected broadcast value" );
			} else {
				if( !Q_stricmp( token.string, "no" ) ) {
					speaker.broadcast = S_BT_LOCAL;
				} else if( !Q_stricmp( token.string, "global" ) ) {
					speaker.broadcast = S_BT_GLOBAL;
				} else if( !Q_stricmp( token.string, "nopvs" ) ) {
					speaker.broadcast = S_BT_NOPVS;
				} else {
					return BG_SS_ParseError( handle, "unknown broadcast value '%s'", token.string );
				}
			}
		} else if( !Q_stricmp( token.string, "wait" ) ) {
			if( !PC_Int_Parse( handle, &speaker.wait ) ) {
				return BG_SS_ParseError( handle, "expected wait value" );
			} else if( speaker.wait < 0 ) {
				return BG_SS_ParseError( handle, "wait value %i is invalid", speaker.wait );
			}
		} else if( !Q_stricmp( token.string, "random" ) ) {
			if( !PC_Int_Parse( handle, &speaker.random ) ) {
				return BG_SS_ParseError( handle, "expected random value" );
			} else if( speaker.random < 0 ) {
				return BG_SS_ParseError( handle, "random value %i is invalid", speaker.random );
			}
		} else if( !Q_stricmp( token.string, "volume" ) ) {
			if( !PC_Int_Parse( handle, &speaker.volume ) ) {
				return BG_SS_ParseError( handle, "expected volume value" );
			} else if( speaker.volume < 0 || speaker.volume > 65535 ) {
				return BG_SS_ParseError( handle, "volume value %i is invalid", speaker.volume );
			}
		} else if( !Q_stricmp( token.string, "range" ) ) {
			if( !PC_Int_Parse( handle, &speaker.range ) ) {
				return BG_SS_ParseError( handle, "expected range value" );
			} else if( speaker.range < 0 ) {
				return BG_SS_ParseError( handle, "range value %i is invalid", speaker.range );
			}			
		} else {
			return BG_SS_ParseError( handle, "unknown token '%s'", token.string );
		}
	}

	if( !BG_SS_StoreSpeaker( &speaker ) ) {
		return BG_SS_ParseError( handle, "Failed to store speaker", token.string );
	}

	return qtrue;
}

qboolean BG_LoadSpeakerScript( const char *filename )
{
	pc_token_t token;
	int handle;

	handle = trap_PC_LoadSource( filename );

	if( !handle )
		return qfalse;

	if( !trap_PC_ReadToken( handle, &token ) || Q_stricmp( token.string, "speakerScript" ) ) {
		return BG_SS_ParseError( handle, "expected 'soundScript'" );
	}

	if( !trap_PC_ReadToken( handle, &token ) || Q_stricmp( token.string, "{" ) ) {
		return BG_SS_ParseError( handle, "expected '{'" );
	}

	while( 1 ) {
		if( !trap_PC_ReadToken( handle, &token ) ) {
			break;
		}

		if( token.string[0] == '}' ) {
			break;
		}

		if( !Q_stricmp( token.string, "speakerDef" ) ) {
			if( !BG_SS_ParseSpeaker( handle ) ) {
				return qfalse;
			}
		} else {
			return BG_SS_ParseError( handle, "unknown token '%s'", token.string );
		}
	}

	trap_PC_FreeSource( handle );

	return qtrue;
}
