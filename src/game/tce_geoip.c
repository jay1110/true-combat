#include "tce_geoip.h"

int TCE_GeoIPParseAddress(const char *text, uint32_t *address)
{
	uint32_t value = 0;
	unsigned part, digits, port;
	int i;
	if (!text || !address)
	{
		return 0;
	}
	for (i = 0; i < 4; ++i)
	{
		part = digits = 0;
		while (*text >= '0' && *text <= '9')
		{
			if (++digits > 3 || (part = part * 10 + (unsigned)(*text++ - '0')) > 255)
			{
				return 0;
			}
		}
		if (!digits || (i < 3 && *text++ != '.'))
		{
			return 0;
		}
		value = (value << 8) | part;
	}
	if (*text == ':')
	{
		++text;
		port = digits = 0;
		while (*text >= '0' && *text <= '9')
		{
			if (++digits > 5 || (port = port * 10 + (unsigned)(*text++ - '0')) > 65535)
			{
				return 0;
			}
		}
		if (!digits)
		{
			return 0;
		}
	}
	if (*text)
	{
		return 0;
	}
	*address = value;
	return 1;
}

int TCE_GeoIPLookup(const unsigned char *database, size_t length, uint32_t address)
{
	const uint32_t countryBegin = UINT32_C(0xffff00);
	uint32_t node = 0;
	int bit;
	if (!database)
	{
		return TCE_COUNTRY_UNKNOWN;
	}
	for (bit = 31; bit >= 0; --bit)
	{
		size_t offset = (size_t)node * 6;
		const unsigned char *branch;
		if (offset > length || length - offset < 6)
		{
			return TCE_COUNTRY_UNKNOWN;
		}
		branch = database + offset + ((address & (UINT32_C(1) << bit)) ? 3 : 0);
		node = (uint32_t)branch[0] | ((uint32_t)branch[1] << 8) | ((uint32_t)branch[2] << 16);
		if (node >= countryBegin)
		{
			return (int)(node - countryBegin);
		}
	}
	return TCE_COUNTRY_UNKNOWN;
}
