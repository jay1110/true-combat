# Omni-bot gameplay

## Enable and verify

Install matching QGame, Omni-bot core and shared runtime scripts/navigation.
Set `omnibot_enable 1` before loading the map. The supplied
`omnibot-release.cfg` configures the packaged runtime.

In the local/server console:

```text
bot help
bot addbot 1 1 BotOne
bot addbot 2 1 BotTwo
```

In a connected client console, use `/bot` when permitted or `/rcon bot ...`
with your configured administrative access. Native help is authoritative for
the current edition. Team values are 1/2; role/weapon interpretation is
TCE-specific. Inspect Deployment and runtime help instead of applying ordinary
ET class assumptions.

## Navigation and missions

The six supplied standard-map sets cover Delta, Northport, Railhouse, Snow,
Stadtrand and Village, with GT2/GT5 mission companions. A map's `.way` is its
navigation graph. Mode-specific `<map>_gtN.gm` and `<map>_gtN_goals.gm` form a
script/goal pair; incomplete pairs must not be mixed with another mode.

Bots use the TCE Use action for appropriate object pickup/activation and
objective operations. GT5 planting/defusing and map announcements connect to
the TCE game state. Tactical CAMP/DEFEND/ATTACK behavior applies only to goals
marked for that map and mode. Carriers remain available for missions; a planted
GT5 bomb takes priority over ordinary camping.

Jumping, ducking, climbing, ladders and switch-controlled doors need matching,
valid navigation/action data. Support in the runtime does not make every
un-authored map traversable. Runtime route costs distribute choices among
existing routes; they do not create new paths through walls or cliffs.

## Traffic and Fireteams

On supported ordinary ground, allied bots try a stable side and validate ground
and full-body clearance. Humans have priority. A blocked bot can relay a short
request for nearby teammates to make room. No safe space means waiting, not
disabled player collisions or teleportation.

Fireteam followers keep formation positions and a stop/resume distance to avoid
pressing directly into their leader. `/bot ft_controls 1` enables their controls;
`0` disables them. Existing bots adopt the setting without a map restart.

Validated door routes reserve one passage at a time. Waiting bots use checked
positions and short safe steps. Precise jump/ladder/door actions retain their
own controllers. An occupied holding goal close to a timed closing door can
still delay egress. See [traffic behavior](bot-traffic.md).

## Game versus Editor

Game includes normal bot gameplay and administration. Waypoint/goal editing,
debug drawing and authoring consoles belong to the Editor edition. See
[authoring](authoring.md). A missing editing command in Game is intentional.

```{toctree}
:maxdepth: 1

bot-traffic
```
