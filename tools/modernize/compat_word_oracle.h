/* Frozen pre-optimization integer algorithms. Test oracle only. */

static __inline void Oracle_TCE_TriggerPlace(tceTriggerSum_t *value,
                                     unsigned long long significand, int shift)
{
	int bit;
	for (bit = 0; bit < 64; ++bit)
	{
		if ((significand >> bit) & 1u)
		{
			value->word[(shift + bit) / 32] |= 1u << ((shift + bit) % 32);
		}
	}
}

static __inline void Oracle_TCE_TriggerRound(tceTriggerSum_t *value, unsigned control)
{
	int highest, drop, bit, word;
	unsigned precision, rounding, discarded = 0, belowHalf = 0, increment = 0;
	unsigned long long wide;
	unsigned carry;

	for (highest = 575; highest >= 0; --highest)
	{
		if (TCE_TriggerBit(value, highest))
		{
			break;
		}
	}
	precision = (control & _MCW_PC) == _PC_24 ? 24u :
	            (control & _MCW_PC) == _PC_53 ? 53u : 64u;
	drop = highest + 1 - (int)precision;
	if (drop <= 0)
	{
		return;
	}
	for (bit = 0; bit < drop; ++bit)
	{
		if (TCE_TriggerBit(value, bit))
		{
			discarded = 1;
			if (bit < drop - 1)
			{
				belowHalf = 1;
			}
		}
	}
	rounding = control & _MCW_RC;
	if (rounding == _RC_NEAR)
	{
		increment = TCE_TriggerBit(value, drop - 1) &&
		            (belowHalf || TCE_TriggerBit(value, drop));
	}
	else if (rounding == _RC_UP)
	{
		increment = discarded && !value->negative;
	}
	else if (rounding == _RC_DOWN)
	{
		increment = discarded && value->negative;
	}
	for (bit = 0; bit < drop; ++bit)
	{
		value->word[bit / 32] &= ~(1u << (bit % 32));
	}
	if (increment)
	{
		word = drop / 32;
		wide = (unsigned long long)value->word[word] + (1u << (drop % 32));
		value->word[word] = (unsigned)wide;
		carry = (unsigned)(wide >> 32);
		while (carry && ++word < 18)
		{
			++value->word[word];
			carry = value->word[word] == 0;
		}
	}
}

static __inline float Oracle_TCE_RotateStore(tceTriggerSum_t value, unsigned control)
{
	unsigned rounding = control & _MCW_RC;
	unsigned bits, discarded = 0, belowHalf = 0, increment = 0, carry;
	unsigned long long wide;
	int highest, drop, bit, word;
	float result;

	for (highest = 575; highest >= 0; --highest)
	{
		if (TCE_TriggerBit(&value, highest))
		{
			break;
		}
	}
	/* Subnormals have a fixed 2^-149 quantum, regardless of magnitude. */
	drop = highest >= 172 ? highest - 23 : 149;
	for (bit = 0; bit < drop; ++bit)
	{
		if (TCE_TriggerBit(&value, bit))
		{
			discarded = 1;
			if (bit < drop - 1)
			{
				belowHalf = 1;
			}
		}
	}
	if (rounding == _RC_NEAR)
	{
		increment = TCE_TriggerBit(&value, drop - 1) &&
		            (belowHalf || TCE_TriggerBit(&value, drop));
	}
	else if (rounding == _RC_UP)
	{
		increment = discarded && !value.negative;
	}
	else if (rounding == _RC_DOWN)
	{
		increment = discarded && value.negative;
	}
	for (bit = 0; bit < drop; ++bit)
	{
		value.word[bit / 32] &= ~(1u << (bit % 32));
	}
	if (increment)
	{
		word = drop / 32;
		wide = (unsigned long long)value.word[word] + (1u << (drop % 32));
		value.word[word] = (unsigned)wide;
		carry = (unsigned)(wide >> 32);
		while (carry && ++word < 18)
		{
			++value.word[word];
			carry = value.word[word] == 0;
		}
	}
	for (highest = 575; highest >= 0; --highest)
	{
		if (TCE_TriggerBit(&value, highest))
		{
			break;
		}
	}
	bits = value.negative << 31;
	if (highest >= 426)
	{
		if (rounding == _RC_NEAR ||
		    (rounding == _RC_UP && !value.negative) ||
		    (rounding == _RC_DOWN && value.negative))
		{
			bits |= 0x7f800000u;
		}
		else
		{
			bits |= 0x7f7fffffu;
		}
	}
	else
	{
		drop = highest >= 172 ? highest - 23 : 149;
		if (highest >= 172)
		{
			bits |= (unsigned)(highest - 171) << 23;
		}
		for (bit = 0; bit < 23; ++bit)
		{
			bits |= TCE_TriggerBit(&value, drop + bit) << bit;
		}
	}
	memcpy(&result, &bits, sizeof(result));
	return result;
}
