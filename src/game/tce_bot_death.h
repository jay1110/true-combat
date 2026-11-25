/* Bot-only death playback timing. No engine ABI or human respawn policy change. */
#ifndef TCE_BOT_DEATH_H
#define TCE_BOT_DEATH_H
#include <stdint.h>
#include <limits.h>
static __inline int TCE_BotDeathDelay(int animationMsec)
{
    return animationMsec>800?animationMsec:800;
}
static __inline int TCE_BotDeathReady(int now,int deadline)
{
    return (uint32_t)((uint32_t)now-(uint32_t)deadline)<=(uint32_t)INT32_MAX;
}
#endif
