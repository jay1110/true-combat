#ifndef OMNIBOT_NAVSTUDIO_DIAGNOSTICS_H
#define OMNIBOT_NAVSTUDIO_DIAGNOSTICS_H

// Navigation authoring diagnostics must not evaluate arguments or emit format
// strings in the public gameplay core. This does not change navigation state.
#ifdef FEATURE_NAVSTUDIO
#define NAVSTUDIO_DIAGNOSTIC(message) EngineFuncs::ConsoleMessage(message)
#else
#define NAVSTUDIO_DIAGNOSTIC(message) ((void)0)
#endif

#endif
