# Omni-bot gameplay runtime

This directory is an explicit standalone build entry for the pinned dependency
under `vendor/omni-bot`. Normal Game builds include its gameplay runtime and the
server bridge by default; NavStudio and waypoint/goal authoring remain disabled.

```
cmake -S omnibot -B build/omnibot-runtime -DOMNIBOT_STD_FILESYSTEM=ON
cmake --build build/omnibot-runtime --config Release
```

Use a separate build directory with `-DFEATURE_NAVSTUDIO=ON` for local authoring.
That build contains the waypoint/goal editor and must not be installed on a
public server. Match the game process architecture. Release tools read the exact output path and architecture from
`omnibot-runtime-<Configuration>.json` in the build directory.
Runtime graph loading, map-trigger connections and gameplay door flags are
independent of the optional authoring interface. Runtime script payloads come
from `vendor/omni-bot/runtime`, with the project TC overlays applied by the release
packager. `omnibot-release.cfg` enables bots before loading the chosen map.
For an explicit modules-only build, use `-DFEATURE_OMNIBOT=OFF` and
`-DTCE_BUILD_OMNIBOT_CORE=OFF`.

Preserve the upstream notices and the unresolved license provenance recorded in
`vendor/omni-bot/IMPORT-NOTICE.txt`. No engine sources are built here.
