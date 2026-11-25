/* Corpse-only terminal selection; never changes death playback or source assets. */
#ifndef TCE_CORPSE_FRAME_H
#define TCE_CORPSE_FRAME_H
static int TCE_CorpseTerminalFrame(int terminal, int previous, float terminalHeadZ,
                                  float previousHeadZ, int tagsValid)
{
    /* Some exported clips append a standing reset frame after the resting pose.
     * Keep the adjacent resting frame only for that abrupt upward reset. */
    if (tagsValid && previousHeadZ < 12.f && terminalHeadZ > previousHeadZ + 16.f)
        return previous;
    return terminal;
}
#endif
