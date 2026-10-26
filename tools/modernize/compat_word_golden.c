#include <stdio.h>
#include <string.h>
#include "../../compat/legacy/game/tce_rotate_precision.h"
#include "compat_word_oracle.h"

static unsigned random_state = 0x5b318e29u;
static unsigned sample(void) { random_state = random_state * 1664525u + 1013904223u; return random_state; }
static unsigned float_bits(float value) { unsigned bits; memcpy(&bits, &value, 4); return bits; }

int main(void)
{
	static const unsigned precisions[] = { _PC_24, _PC_53, _PC_64 };
	static const unsigned modes[] = { _RC_NEAR, _RC_DOWN, _RC_UP, _RC_CHOP };
	tceTriggerSum_t value, expected, actual;
	unsigned long long significand;
	unsigned control, old_bits, new_bits;
	int c, p, r, word, shift, bit;
	unsigned long checks = 0;
	for (c = 0; c < 20000; ++c)
	{
		for (word = 0; word < 18; ++word) value.word[word] = c % 4 ? sample() : 0;
		value.negative = sample() & 1u;
		/* Dense arbitrary states, sparse carries, ties, zero, and each edge. */
		if (!(c % 4)) value.word[(c / 4) % 18] = 1u << ((c / 72) % 32);
		if (!(c % 17)) memset(value.word, 0, sizeof(value.word));
		if (!(c % 23)) memset(value.word, 255, sizeof(value.word));
		for (p = 0; p < 3; ++p) for (r = 0; r < 4; ++r)
		{
			control = precisions[p] | modes[r];
			expected = actual = value;
			Oracle_TCE_TriggerRound(&expected, control);
			TCE_TriggerRound(&actual, control);
			if (memcmp(&expected, &actual, sizeof(actual)))
			{
				fprintf(stderr, "round mismatch case=%d precision=%d rounding=%d\n", c, p, r); return 1;
			}
			old_bits = float_bits(Oracle_TCE_RotateStore(value, control));
			new_bits = float_bits(TCE_RotateStore(value, control));
			if (old_bits != new_bits)
			{
				fprintf(stderr, "store mismatch case=%d precision=%d rounding=%d bits=%08x/%08x\n", c, p, r, old_bits, new_bits); return 1;
			}
			checks += 2;
		}
		shift = c % 576;
		significand = ((unsigned long long)sample() << 32) | sample();
		if (shift > 512) significand &= (1ULL << (576 - shift)) - 1;
		expected = actual = value;
		Oracle_TCE_TriggerPlace(&expected, significand, shift);
		TCE_TriggerPlace(&actual, significand, shift);
		if (memcmp(&expected, &actual, sizeof(actual)))
		{
			fprintf(stderr, "place mismatch case=%d shift=%d\n", c, shift); return 1;
		}
		++checks;
	}
	/* Every grid boundary, including zero placements beyond occupied words. */
	for (shift = 0; shift < 576; ++shift) for (bit = 0; bit < 64 && shift + bit < 576; ++bit)
	{
		memset(&expected, 0, sizeof(expected)); actual = expected;
		Oracle_TCE_TriggerPlace(&expected, 1ULL << bit, shift);
		TCE_TriggerPlace(&actual, 1ULL << bit, shift);
		if (memcmp(&expected, &actual, sizeof(actual))) return 1;
		++checks;
	}
	printf("%lu bit-exact previous-helper comparisons passed (PC24/53/64, all four RC modes).\n", checks);
	return 0;
}
