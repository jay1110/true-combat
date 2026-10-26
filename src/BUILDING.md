# Build entry point

The current mod build, target matrix, optional editor/runtime split, canonical
versioned packages and validation commands are documented in
[the root build guide](../BUILDING.md).

The three production libraries remain cgame, ui and qagame. Direct CMake
configuration with -S src is retained for existing build caches; new builds
should configure the repository root.
