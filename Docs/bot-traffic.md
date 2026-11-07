# Bot traffic in TCE2

The Game and Editor variants use the same gameplay traffic rules. No waypoint
editor, console drawing commands or authoring interface is enabled by these
rules.

## Giving way

On ordinary supported ground, bots detect nearby living teammates. Humans have
priority. Bots try a stable passing side, checking both the complete player
volume and the ground before stepping aside. If neither side is safe, they stop;
they do not apply a physics impulse to the player. Opponents, spectators, dead
players and players on another floor do not create a friendly passing request.

Exact door actions, ladders, jumps and other precise traversal keep their own
controllers. Avoidance does not create navigation links or permit walking over
an unverified ledge.

## Following

Fireteam followers retain a stable formation slot. They stop closing when the
leader is nearer than 96 game units, cancel the current follow path, and resume
after the distance exceeds 128 units. The different stop/resume distances prevent
repeated movement changes at a single distance boundary. A leader can still walk
into a follower; ordinary safe yielding handles that encounter.

Use `/bot ft_controls 1` to enable fireteam orders. Existing bots pick up this
setting in their next priority update, within about one second; restarting the
map is unnecessary. `/bot ft_controls 0` disables the controls again.

## Doors

Generated navigation with a validated TCE door recipe reserves one passage at a
time. Waiting bots use checked positions away from the entrance. Requests are
ordered by arrival, with client ID resolving simultaneous arrivals. A human in
the doorway or either approach has priority; a bot already crossing continues
to clear the opening.

Switch-operated doors use the existing switch route while holding their turn.
The queue does not unlock doors, open a mover through script, or guess a route
through missing navigation. Death, disconnection, path reset, expired requests
and map time changes release reservations. Waiting and passage reservations are
bounded so an unavailable route cannot hold a door indefinitely.

These rules do not certify every map. A missing or invalid door recipe still
requires navigation generated for that mover. Existing ET navigation without
TCE recipes retains its original traversal behavior.
