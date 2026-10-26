"""Geometry contracts for TC objective use; no floor or route is invented.

GT5 plants use the snapped actor origin's 24x24x20 query in fire_grenade.
G_TouchTriggers separately requires feet+4 below the objective trigger top.
These necessary geometric conditions do not prove a clear aim ray, cursor
hint, successful use animation, or a completed live objective.
"""
import math


def valid_bounds(bounds):
    return (isinstance(bounds, (tuple, list)) and len(bounds) == 2
            and all(isinstance(v, (tuple, list)) and len(v) == 3 for v in bounds)
            and all(type(v) in (int, float) and math.isfinite(v) for row in bounds for v in row)
            and all(a <= b for a, b in zip(*bounds)))


def overlap(a, b):
    # Positive overlap avoids relying on the engine's padded abs-bounds at an
    # exact zero-area boundary. Real native validation is still mandatory.
    return all(x < upper and lower < y for x, y, lower, upper in
               zip(a[0], a[1], b[0], b[1]))


def aimed_surface_distance(eye, bounds):
    """First AABB face on the actual center-aim direction, not center distance."""
    center=[(a+b)*.5 for a,b in zip(*bounds)]
    delta=[b-a for a,b in zip(eye,center)]
    entry,exit=0.,1.
    for axis in range(3):
        if delta[axis]==0:
            if not bounds[0][axis]<=eye[axis]<=bounds[1][axis]: return math.inf
            continue
        times=[(bounds[i][axis]-eye[axis])/delta[axis] for i in (0,1)]
        entry=max(entry,min(times));exit=min(exit,max(times))
    return entry*math.sqrt(sum(v*v for v in delta)) if entry<=exit else math.inf


def capture_contact(feet, trigger_bounds, radius=16, height=87):
    """Conservative capsule contact with a flagonly trigger's bounds.

    Capture calls EntityContactCapsule with the player's full body. The
    origin need not be inside the trigger: a solid flagpost often occupies
    its centre. The capsule is a vertical segment swept by the hull radius,
    so corners and floors on another storey cannot pass an expanded XY box.
    The 2.125-unit inset covers runtime link-bounds padding and movement skin;
    this remains a proposal, requiring native floor/route and live capture.
    """
    if (not valid_bounds(trigger_bounds) or len(feet)!=3 or
            any(type(v) not in (int,float) or not math.isfinite(v) for v in feet) or
            not math.isfinite(radius) or not math.isfinite(height) or
            radius<=2.125 or height<2*radius):
        return False
    distance_sq=sum(max(low-point,0.,point-high)**2 for point,low,high in
                    zip(feet[:2],trigger_bounds[0][:2],trigger_bounds[1][:2]))
    bottom,top=feet[2]+radius,feet[2]+height-radius
    distance_sq+=max(trigger_bounds[0][2]-top,0.,bottom-trigger_bounds[1][2])**2
    return distance_sq<(radius-2.125)**2


def flag_pickup_contact(origin,flag_origin):
    """Existing TC Flag UsePosition contract: BG_PlayerTouchesItem's36u box.

    Reserve the same2.125u final arrival/movement margin used by goal_flag.gm
    TCEFlagAtCheckedBase. A valid proposal still needs actual held USE and
    native ownership/capture events before a live pickup can be claimed.
    """
    return (isinstance(origin,(list,tuple))and isinstance(flag_origin,(list,tuple))and
        len(origin)==len(flag_origin)==3 and all(type(v)in(int,float)and math.isfinite(v)for v in list(origin)+list(flag_origin))and
        all(abs(a-b)<=36-2.125 for a,b in zip(origin,flag_origin)))


def plant_contact(feet, origin_above_feet, trigger_bounds, radius=16, height=87,
                  target_bounds=None, contact_top=None):
    if (not valid_bounds(trigger_bounds) or len(feet) != 3
            or any(not math.isfinite(v) for v in feet)):
        return False
    if contact_top is None:contact_top=trigger_bounds[1][2]
    if not math.isfinite(contact_top) or feet[2] + 4 >= contact_top:
        return False
    # q_shared.h SnapVector explicitly casts to int (toward zero).
    origin = [int(feet[0]), int(feet[1]), int(feet[2] + origin_above_feet)]
    planted_box = [[origin[0]-12, origin[1]-12, origin[2]],
                   [origin[0]+12, origin[1]+12, origin[2]+20]]
    if not overlap(planted_box, trigger_bounds):
        return False
    if target_bounds is not None:
        if not valid_bounds(target_bounds):
            return False
        # This target may be removed by GT2's mapscript while its GT5 crate
        # is solid. Keep the activation radius2 plus movement skin. A GT2
        # trace that just touches the absent crate is not GT5 clearance.
        clearance = radius + 2.125
        body = [[feet[0]-clearance, feet[1]-clearance, feet[2]],
                [feet[0]+clearance, feet[1]+clearance, feet[2]+height]]
        if overlap(body, target_bounds):
            return False
        # GM acquires the hint crouched. Native G_CheckForCursorHints limits
        # this exact bomb-target hint to40 units from the eye to the trace hit.
        # Standing on a48-unit crate puts the crouched eye56 units above its
        # top and can never get that hint, even if the wider TOI is touched.
        eye=[feet[0],feet[1],feet[2]+origin_above_feet+(26 if origin_above_feet==30 else 21)]
        if aimed_surface_distance(eye,target_bounds)>40:
            return False
    return True


def plant_perimeter_axes(low, high, target_low=None, target_high=None, radius=16):
    """Proposal coordinates; the caller must find and natively prove a floor."""
    middle = (low+high)*.5
    width = high-low
    values = {middle, low+min(16., width*.25), high-min(16., width*.25),
              low-8., high+8.}
    for delta in (-64, -32, 32, 64):
        if low-8. <= middle+delta <= high+8.:
            values.add(middle+delta)
    if (type(target_low) in (int,float) and type(target_high) in (int,float) and
            math.isfinite(target_low) and math.isfinite(target_high) and target_low<=target_high):
        # The TOI can be much larger or offset from its small crate. Its
        # 32-unit lattice may land either inside the solid target or beyond
        # the actual 40-unit cursor-hint reach. Propose full-body-clear sides
        # of the target itself, then keep the unchanged plant/contact gate.
        values.add((target_low+target_high)*.5)
        # Runtime linked bounds extend the BSP target by two units. Keep
        # strictly beyond that plus the unchanged 2.125 movement inset.
        for extra in (4.5,8.,12.):
            for point in (target_low-radius-extra,target_high+radius+extra):
                if low-8.<=point<=high+8.:values.add(point)
    return sorted(values)
