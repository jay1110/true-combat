# Client platform advertisement

`g_oss` advertises supported client operating systems and architectures in the
server information used by compatible server browsers. It does not enable
gameplay features, change collisions, reject connections or install client modules.
It is editable server information, matching Xmod's `CVAR_SERVERINFO` behavior.

The default is **511** for the complete TCE2 release matrix. Add the flags for
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

Xmod's default `2047` includes all eleven flags, including Android x86/x86_64.
Those two targets are absent from the current TCE2 release matrix, so TCE2 does
not advertise them by default. Administrators may explicitly set `g_oss 2047`
when they supply the corresponding client binaries. If installing a diagnostic
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
