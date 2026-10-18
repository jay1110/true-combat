#ifndef TCE_NAV_WATER_H
#define TCE_NAV_WATER_H
/* PM_SetWaterLevel's integer middle sample, measured above the feet.
 * ClientSpawn configures (-24,view42/21) or (-30,view52/26).
 * A wet first sample and dry middle sample is grounded waterlevel1;
 * swimming (middle sample wet), lava and slime remain unsupported. */
static __inline float TCE_NavWaterMiddleSample(int enlarged,int crouched) {
 int origin=enlarged?30:24,view=crouched?(enlarged?26:21):(enlarged?52:42);
 return (float)((view+origin)/2);
}
#endif
