# Architecture and source provenance

## Mod boundary

| Component | Responsibility |
| --- | --- |
| `qagame` | Authoritative server gameplay, damage, spawning, missions, Lua and bot bridge. |
| `cgame` | Client prediction/presentation, HUD, effects and native match screens. |
| `ui` | Menus, profiles, server browser and appearance configuration. |
| Omni-bot core | Gameplay AI, paths, goals and the compatible GameMonkey runtime. |
| External engine | Networking, rendering/audio, filesystem, module loading and engine syscalls. |

The public entry points remain `dllEntry` and `vmMain`. Different native/WASM
targets must follow the matching host ABI; native libraries cannot be exchanged
merely because their filenames look similar.

## Source organization

| Directory | Purpose |
| --- | --- |
| `src` | Reconstructed TC:E-specific implementation, retained SDK code and tests. |
| `compat` | Explicit legacy numeric/ABI compatibility helpers. |
| `platform` | Actual OS services such as native window integration. |
| `vendor` | Third-party source/runtime inputs and component notices. |
| `pak` | Production mod resources included in the client package. |
| `omnibot`, `navstudio` | Bot integration and optional authoring tools. |
| `tools`, `cmake` | Build, validation, packaging and documentation support. |

## Reconstruction versus original sources

The TC:E-specific proprietary implementation was rebuilt using binary analysis
and behavioral evidence. Publishing this reconstruction does not turn it into
the original TC:E authors' source release. SDK-derived portions and imported
libraries are distinct and retain their provenance; it would also be inaccurate
to describe every file in this repository as newly written TCE2 code.

Compatibility profiles preserve relevant original Windows/Linux arithmetic and
ABI behavior where supported by evidence. They are not a blanket guarantee of
parity. Known limitations and component origins are documented in
[credits](credits.md), [troubleshooting](troubleshooting.md) and the source notices.

## Data manifests

Build configuration, runtime policies, asset inventories and dependency pins are
source inputs. Generated machine-specific reports are not required publication
inputs. Public provenance uses repository-relative paths or identifiable upstream
URLs; it must not depend on a contributor's personal checkout path.

```{toctree}
:maxdepth: 1

dead-code-audit
```
