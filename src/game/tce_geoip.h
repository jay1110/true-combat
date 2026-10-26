#ifndef TCE_GEOIP_H
#define TCE_GEOIP_H

#include <stddef.h>
#include <stdint.h>

#define TCE_COUNTRY_UNKNOWN 255
#define TCE_COUNTRY_LOCAL   254

/* IPv4 legacy GeoIP country databases and the xmod 16x16 flag atlas use
 * matching numeric country IDs. Invalid/missing data resolves to unknown. */
int TCE_GeoIPParseAddress(const char *text, uint32_t *address);
int TCE_GeoIPLookup(const unsigned char *database, size_t length, uint32_t address);

void G_GeoIPInit(void);
void G_GeoIPShutdown(void);
int G_GeoIPCountry(const char *address, int isBot);

#endif
