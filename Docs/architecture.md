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

## Wall-weapon presentation

Wall clearance is calculated in shared movement and smoothed in command time. The upper byte of the existing signed-short `stats[15]` carries the obstruction amount; its equipment byte remains intact. Both player-state converters publish the resulting low-ready pitch in the previously unused `ET_PLAYER angles2[PITCH]` component. Movement yaw and lean keep their existing components. Engine player/entity layouts are unchanged.

Standing CGame presentation blends toward the declared `relaxed_idle_1h_1` pose. Crouching uses the arm-only `wall_high_ready.mdx` pose, generated from the original ET skeleton with fixed limb lengths and unchanged head/chest channels. Prone presentation retains its original animation frames, rotates the upper body toward the nearest clear side and uses the existing back attachment while crawling or when neither side has room. Shared movement performs the clearance search and bounded return; the signed yaw/stow state uses the same leased stat byte and entity component. Leg animation and normal look axes are retained. It uses animation metadata rather than stock frame numbers. The attached world weapon keeps its model scale and follows the stance-specific aim rule as QGame. Negotiated affected-pose hit evaluation applies the same torso-frame blend, including in historical queries and cache keys. The presentation profile version participates in asset negotiation so older pose policies cannot silently negotiate as identical.

## Data manifests

Build configuration, runtime policies, asset inventories and dependency pins are
source inputs. Generated machine-specific reports are not required publication
inputs. Public provenance uses repository-relative paths or identifiable upstream
URLs; it must not depend on a contributor's personal checkout path.

```{toctree}
:maxdepth: 1

dead-code-audit
```

The head clearance sweep is shared by server movement and client prediction; any correction moves the authoritative player origin through a checked body sweep. Eye and grip probes use the same asymmetric lean offset as the camera and shot origin. Crouching advertises a negative ready pitch; prone stow/crawl firing is suppressed in shared weapon processing and guarded again before server weapon effects. A visible side-deflected weapon remains usable; both muzzle axes and actual pre-recoil bullet endpoints include its yaw. First-person crouch high-ready placement keeps the view model sleeve ends below the camera.
