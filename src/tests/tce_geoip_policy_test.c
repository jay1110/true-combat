#include "../game/g_local.h"
#include "../game/tce_geoip.h"

vmCvar_t g_countryflags;
static char serverAddress[128];
static int databaseAvailable = 1, opens;

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "GeoIP policy failed line%d: %s\n", __LINE__, #c); return 1; } } while (0)
void QDECL G_Printf(const char *format, ...) { (void)format; }
int Q_stricmp(const char *a, const char *b)
{
	unsigned char x, y;
	do { x = (unsigned char)*a++; y = (unsigned char)*b++;
		if (x >= 'A' && x <= 'Z') { x += 'a' - 'A'; }
		if (y >= 'A' && y <= 'Z') { y += 'a' - 'A'; }
		if (x != y) { return x - y; }
	} while (x);
	return 0;
}
int trap_FS_FOpenFile(const char *name, fileHandle_t *file, fsMode_t mode)
{
	(void)name; (void)mode; ++opens; *file = databaseAvailable ? 1 : 0;
	return databaseAvailable ? 6 : -1;
}
void trap_FS_Read(void *buffer, int length, fileHandle_t file)
{
	unsigned char data[6] = { 57, 255, 255, 175, 255, 255 };
	(void)file; memcpy(buffer, data, (size_t)length);
}
void trap_FS_FCloseFile(fileHandle_t file) { (void)file; }
void trap_Cvar_VariableStringBuffer(const char *name, char *buffer, int size)
{
	(void)name; snprintf(buffer, (size_t)size, "%s", serverAddress);
}

int main(void)
{
	int i, country, seen[244] = { 0 }, distinct = 0;
	g_countryflags.integer = 0;
	CHECK(G_GeoIPCountry("8.8.8.8", 0) == -1 && G_GeoIPCountry("bot", 1) == -1 && !opens);
	g_countryflags.integer = 1;
	CHECK(G_GeoIPCountry("localhost", 0) == TCE_COUNTRY_LOCAL);
	CHECK(G_GeoIPCountry("127.0.0.1:27960", 0) == TCE_COUNTRY_LOCAL);
	CHECK(G_GeoIPCountry("::1", 0) == TCE_COUNTRY_LOCAL);
	CHECK(G_GeoIPCountry("[::1]", 0) == TCE_COUNTRY_LOCAL);
	CHECK(G_GeoIPCountry("192.168.178.81:27960", 0) == TCE_COUNTRY_UNKNOWN && !opens);
	CHECK(G_GeoIPCountry("8.8.8.8:27960", 0) == 57 && opens == 1);
	strcpy(serverAddress, "200.1.2.3"); CHECK(G_GeoIPCountry("bot", 1) == 175 && opens == 1);
	strcpy(serverAddress, "localhost"); CHECK(G_GeoIPCountry("bot", 1) == TCE_COUNTRY_LOCAL);
	strcpy(serverAddress, "0.0.0.0"); CHECK(G_GeoIPCountry("bot", 1) == TCE_COUNTRY_UNKNOWN);
	G_GeoIPShutdown(); opens = 0; databaseAvailable = 0;
	CHECK(G_GeoIPCountry("8.8.8.8", 0) == TCE_COUNTRY_UNKNOWN && opens == 1);
	CHECK(G_GeoIPCountry("8.8.8.8", 0) == TCE_COUNTRY_UNKNOWN && opens == 1);
	g_countryflags.integer = 2; srand(42);
	for (i = 0; i < 10000; ++i)
	{
		country = G_GeoIPCountry("bot", 1);
		CHECK(country >= 3 && country <= 243);
		if (!seen[country]++) { ++distinct; }
	}
	CHECK(distinct == 241 && opens == 1);
	CHECK(G_GeoIPCountry("localhost", 0) == TCE_COUNTRY_LOCAL);
	G_GeoIPShutdown();
	puts("PASS GeoIP policy: modes0/1/2, local/LAN/public/missing database, server net_ip, 10000 bot samples");
	return 0;
}
