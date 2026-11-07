# Client platform advertisement

`g_oss` advertises supported client operating systems and architectures in the
server information used by compatible server browsers. It does not enable
gameplay features, change collisions, reject connections or install client modules.
It is editable server information, matching Xmod's `CVAR_SERVERINFO` behavior.

The default is **2047** for the complete TCE2 release matrix. Add the flags for
the client binaries actually supplied by the server:

| Flag | Client platform |
| --- | --- |
| 1 | Windows x86 |
| 2 | Linux i386 |
| 4 | Linux x86_64 |
| 8 | macOS x86_64 |
| 16 | Android ARM64 |
| 32 | Linux ARMv7 |
| 64 | Linux ARM64 |
| 128 | macOS ARM64 |
| 256 | Windows x86_64 |
| 512 | Android x86 |
| 1024 | Android x86_64 |

`0` means unknown/legacy support; the browser may apply its own fallback.
The existing convention has no flag for Android ARMv7 or WebAssembly. TCE2 builds
those targets, but does not invent incompatible bits for them. The macOS universal
binary supplies both macOS flags.

The default `2047` includes all eleven flags, including Android x86/x86_64.
All eleven represented platforms have client build targets; the macOS universal
target covers two flags. This is a build/package capability declaration, not
proof of runtime parity on every device. If installing a diagnostic
package with fewer ABIs, advertise only its installed clients; the usual Windows
x86/x86_64 plus Linux i386/x86_64 subset is `263`.

Primary references:

- [Xmod flag definitions](https://github.com/jay1110/xmod/blob/master/src/game/g_xmod.h)
- [Xmod registration](https://github.com/jay1110/xmod/blob/master/src/game/g_main.cpp)
- [Xmod cvar documentation](https://github.com/jay1110/xmod/blob/master/doc/cvar/cvar.g_oss.xml)
- [ETLegacy browser convention](https://github.com/etlegacy/etlegacy/blob/master/src/qcommon/q_oss.h)

The source definition and registration determine the behavior; the historical
Xmod documentation's automatic-detection wording does not imply that the cvar
can discover which client binaries an administrator has installed.

## Android x86 build targets

The release matrix includes separate `android-x86` and `android-x86_64` targets.
Their NDK ABIs are `x86` and `x86_64`, with Android API23 by default, matching the
existing ARM target minimum. Set `ANDROID_NDK_HOME` to an installed NDK (the manual
workflow pins27.3.13750724), then use a separate build directory per ABI:

```sh
cmake --preset android-x86
cmake --build --preset android-x86
cmake --preset android-x86_64
cmake --build --preset android-x86_64
```

These normal presets build UI, CGame, QGame and the gameplay Omni-bot core.
Authoring remains an explicit `FEATURE_NAVSTUDIO=ON` variant. Client module names
use `lib<module>.mp.android.<ABI>.so`; the matching bot core is
`omnibot_et.android.<ABI>.so`. QGame's loader selects the same Android suffix,
including on x86 CPUs, instead of trying a Linux core name.

An Android engine is required to load these mod libraries. There is no APK,
Android engine port or emulator bundled here. Source/configuration and package
checks do not replace an actual NDK build or device runtime test.

See the official [Android ABI list](https://developer.android.com/ndk/guides/abis)
and [NDK CMake instructions](https://developer.android.com/ndk/guides/cmake).
