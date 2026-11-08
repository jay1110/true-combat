# Game modes and controls

## Game modes

| `g_gametype` | TCE2 mode | Main behavior |
| --- | --- | --- |
| `2` | Reinforced Objective | Map-defined objectives with reinforcement/respawn behavior. |
| `5` | Objective | Round-based objective/bomb play, as defined by the map. |
| `7` | Bodycount | Bodycount play; optional map-authored deathmatch spawn positions. |

These numbers describe **TCE2**, not the similarly numbered modes of ordinary ET.
Not every map supplies every mode. Select the mode at process/map startup with
`+set g_gametype N`.

## Controls

Use **Options -> Controls** to inspect and change your actual bindings.
Profiles may override defaults. Named actions include Attack, Aim / Iron Sights,
Reload Weapon, Switch Firemode, Jump, Crouch, Prone, Use (Activate), Deploy Menu,
Scores, Weapon Stats, Objective and Drop Objective.

The native **Use/Activate** action, conventionally bound to F, also drives
relevant bot object and switch interactions. A weapon change during reload
must not grant ammunition or carry another weapon's reload state.

**Scores** retains its hold-to-show behavior. The Ingame menu's **Scoreboard**
opens a persistent scoreboard; Escape closes it. Weapon Stats is independently
bindable and reports TCE weapon data.

## Teams and deployment

The Deployment menu selects team, role and available equipment. Choices depend
on the server/team. Weapon icons and names come from TCE definitions; selections
do not bypass server loadout restrictions.

Team labels are white; your own Fireteam uses light green. Labels sit above the
head and fade briefly on loss of visibility. Crosshair names and overhead team
names have separate HUD options.

## Damage and spawning

Confirmed bullet contacts use regional head/body/leg damage without extra
weakening near the region's edge. Cover and hit-registration/antilag settings
still apply; not every visible shot necessarily intersects a target.

GT7 custom spawns, when enabled by the mapscript, favor the largest distance to
the nearest living opponent among suitable points. The server handles occupied
points and fallback spawns. GT7 spawn immunity does not apply the former extra
ROE punishment to an attacker shooting a protected enemy.
