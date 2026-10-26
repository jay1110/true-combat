#include "g_local.h"
#include "tce_geoip.h"

/* This is server-only data. No database or address is sent to clients. */
static unsigned char *countryDatabase;
static size_t countryDatabaseLength;
static int countryDatabaseAttempted;

void G_GeoIPShutdown(void)
{
	free(countryDatabase);
	countryDatabase = NULL;
	countryDatabaseLength = 0;
	countryDatabaseAttempted = 0;
}

void G_GeoIPInit(void)
{
	fileHandle_t file = 0;
	int length;
	if (countryDatabaseAttempted || !g_countryflags.integer)
	{
		return;
	}
	countryDatabaseAttempted = 1;
	length = trap_FS_FOpenFile("GeoIP.dat", &file, FS_READ);
	if (length < 6 || length > 16 * 1024 * 1024 || !file)
	{
		if (file)
		{
			trap_FS_FCloseFile(file);
		}
		G_Printf("GeoIP: no valid GeoIP.dat found; country flags use unknown/local only.\n");
		return;
	}
	countryDatabase = (unsigned char *)malloc((size_t)length);
	if (countryDatabase)
	{
		trap_FS_Read(countryDatabase, length, file);
		countryDatabaseLength = (size_t)length;
		G_Printf("GeoIP: loaded IPv4 country database (%d bytes).\n", length);
	}
	else
	{
		G_Printf("GeoIP: country database allocation failed.\n");
	}
	trap_FS_FCloseFile(file);
}

int G_GeoIPCountry(const char *address, int isBot)
{
	uint32_t number;
	int country;
	char serverAddress[MAX_CVAR_VALUE_STRING];
	if (!g_countryflags.integer)
	{
		return -1;
	}
	if (isBot)
	{
		/* Bots inherit the server location, never a fabricated nationality. */
		trap_Cvar_VariableStringBuffer("net_ip", serverAddress, sizeof(serverAddress));
		address = serverAddress;
	}
	if (address && !Q_stricmp(address, "localhost"))
	{
		return TCE_COUNTRY_LOCAL;
	}
	if (!TCE_GeoIPParseAddress(address, &number))
	{
		return TCE_COUNTRY_UNKNOWN;
	}
	if ((number >> 24) == 127)
	{
		return TCE_COUNTRY_LOCAL;
	}
	if (!number || (number >> 24) == 10 || (number >> 16) == UINT32_C(0xc0a8) ||
	    (number >> 20) == UINT32_C(0xac1) || (number >> 16) == UINT32_C(0xa9fe) ||
	    (number >> 28) >= 14)
	{
		return TCE_COUNTRY_UNKNOWN;
	}
	G_GeoIPInit();
	country = TCE_GeoIPLookup(countryDatabase, countryDatabaseLength, number);
	return country > 0 && country < TCE_COUNTRY_LOCAL ? country : TCE_COUNTRY_UNKNOWN;
}
