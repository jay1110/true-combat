#ifndef OMNIBOT_NAVSTUDIO_ADAPTER_H
#define OMNIBOT_NAVSTUDIO_ADAPTER_H

// Original ET modules expose the normal Omni engine interface. These commands
// use that interface and never require TC-specific qagame exports.
namespace NavStudioAdapter {
void Snapshot(const StringVector &args);
void Import(const StringVector &args);
void Update();
}

#endif
