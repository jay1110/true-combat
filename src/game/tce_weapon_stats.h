#ifndef TCE_WEAPON_STATS_H
#define TCE_WEAPON_STATS_H

/* Opt-in text protocol; no playerState/entityState or SDK ABI fields. */
#define TCE_WSTATS_ROWS 64
#define TCE_WSTATS_PACKET_ROWS 8
#define TCE_WSTATS_HEADER_ARGS 9
typedef struct {
	int attacks, hits, kills, deaths, headshots;
} tce_weaponStatsRow_t;

#endif
