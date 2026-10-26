#include "../game/tce_geoip.h"
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "GeoIP check failed at line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

static void leaf(unsigned char *out, unsigned country)
{
	unsigned value = 0xffff00u + country;
	out[0] = (unsigned char)value; out[1] = (unsigned char)(value >> 8); out[2] = (unsigned char)(value >> 16);
}

int main(void)
{
	unsigned char database[12] = { 0 };
	uint32_t address = 0;
	const char *invalid[] = { "", "1.2.3", "1.2.3.4.5", "256.2.3.4", "1..3.4", "1.2.3.4:", "1.2.3.4:65536", "1.2.3.4:123x", "1.2.3.4x", "localhost", "::1", "1.2.3.-1", "1234567890123456789.2.3.4" };
	unsigned i;
	CHECK(TCE_GeoIPParseAddress("1.2.3.4", &address) && address == UINT32_C(0x01020304));
	CHECK(TCE_GeoIPParseAddress("255.254.253.252:27960", &address) && address == UINT32_C(0xfffefdfc));
	CHECK(TCE_GeoIPParseAddress("0.0.0.0:0", &address) && address == 0);
	for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) { CHECK(!TCE_GeoIPParseAddress(invalid[i], &address)); }
	CHECK(!TCE_GeoIPParseAddress(NULL, &address) && !TCE_GeoIPParseAddress("1.2.3.4", NULL));
	leaf(database, 57); leaf(database + 3, 175);
	CHECK(TCE_GeoIPLookup(database, 6, 0) == 57);
	CHECK(TCE_GeoIPLookup(database, 6, UINT32_C(0x80000000)) == 175);
	/* Second-level branch verifies bit31/30 and little-endian node pointers. */
	database[0] = 1; database[1] = database[2] = 0;
	leaf(database + 6, 38); leaf(database + 9, 75);
	CHECK(TCE_GeoIPLookup(database, sizeof(database), 0) == 38);
	CHECK(TCE_GeoIPLookup(database, sizeof(database), UINT32_C(0x40000000)) == 75);
	CHECK(TCE_GeoIPLookup(database, 6, 0) == TCE_COUNTRY_UNKNOWN);
	CHECK(TCE_GeoIPLookup(NULL, 0, 0) == TCE_COUNTRY_UNKNOWN);
	memset(database, 0, sizeof(database));
	CHECK(TCE_GeoIPLookup(database, sizeof(database), 0) == TCE_COUNTRY_UNKNOWN);
	database[0] = database[1] = database[2] = 0xfe;
	CHECK(TCE_GeoIPLookup(database, sizeof(database), 0) == TCE_COUNTRY_UNKNOWN);
	puts("PASS GeoIP: strict IPv4/port parser, country leaves, high address bits, truncated/cyclic/out-of-range databases");
	return 0;
}
