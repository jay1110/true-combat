#ifndef TCE_COUNTRYFLAGS_H
#define TCE_COUNTRYFLAGS_H

/* Configstring IDs 254/255 describe local/unknown connections. They are not
 * atlas coordinates: the imported xmod atlas contains national artwork at
 * those coordinates. Its question-mark cell is 1. Disabled flags use -1. */
static int TCE_CountryFlagAtlasIndex(int country)
{
	if (country < 0 || country > 255)
	{
		return -1;
	}
	return country == 0 || country >= 254 ? 1 : country;
}

#endif
