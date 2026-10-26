"""Append explicit, natively proved door recipes without replacing user work."""
import copy
import math

from edit import action_protection


def merge(doc, report, origin, baseline=None):
    if (report.get('map') != doc['map'] or report.get('generation_complete') is not True or
            report.get('door_generation', {}).get('explicit_repair') is not True):
        raise RuntimeError('Türreparatur hat keinen vollständigen aktuellen nativen Beleg geliefert.')
    original = {n['id']: n for n in doc['nodes']}
    rows = {n['id']: n for n in report.get('nodes', []) if n.get('accepted') is True}
    previous={n['id']:n for n in (baseline or {}).get('nodes',[]) if n.get('accepted') is True}
    if baseline is not None and (baseline.get('generation_complete') is not True or
                                baseline.get('map')!=doc['map'] or not set(original)<=set(previous)):
        raise RuntimeError('Vollständige native Ausgangsprüfung der Türreparatur fehlt.')
    if not set(original) <= set(rows):
        raise RuntimeError('Türreparatur würde vorhandene Punkte entfernen; Entwurf bleibt erhalten.')
    for identity, old in original.items():
        new = rows[identity]
        raw=list(new.get('raw_position',new['position']));feet=[raw[0],raw[1],raw[2]-origin]
        # Recast samples can legitimately project onto the exact native tread.
        # Bind the literal to the draft, and the resulting floor to the current
        # certified report. Neither coordinate is adopted into the user draft.
        projection_changed=(math.dist(new['position'],previous[identity]['position'])>.011) if baseline is not None else False
        if (math.dist(feet, old['position']) > .011 or projection_changed or
                int(new.get('nav_flags', 0)) != int(old['nav_flags']) or
                old.get('way_guid') and new.get('way_guid') != old['way_guid']):
            raise RuntimeError('Türreparatur hat einen vorhandenen Punkt verändert; Entwurf bleibt erhalten.')
    before = {(e['from'], e['to']) for e in doc['edges']}
    after = {(e['from'], e['to']) for e in report.get('edges', []) if e.get('accepted')}
    if not before <= after:
        raise RuntimeError('Türreparatur würde vorhandene Verbindungen entfernen; Entwurf bleibt erhalten.')
    added = set(rows) - set(original)
    guides=set(report['door_generation'].get('approach_source_ids',[]))
    def proved_addition(identity):
        node=rows[identity];recipe=node.get('door_recipe')
        if recipe:return recipe.get('validated') is True and recipe.get('exported') is True
        return (identity in guides and node.get('reason')=='generated_door_approach_live_pending' and
                node.get('topology_protected') is True and node.get('radius',0)>=10 and
                int(node.get('nav_flags',0)) in (0,64))
    if any(i < doc['next_id'] or not proved_addition(i) for i in added):
        raise RuntimeError('Türreparatur enthält fremde oder wiederverwendete Punkt-IDs.')
    if any(a not in added and b not in added for a, b in after - before):
        raise RuntimeError('Türreparatur enthält unerwartete gewöhnliche Verbindungen.')
    result = copy.deepcopy(doc)
    protection = action_protection(report)
    for identity in sorted(added):
        source = rows[identity]
        feet = list(source['position']); feet[2] -= origin
        flags = int(source.get('nav_flags', 0))
        result['nodes'].append(dict(id=identity, position=feet, radius=source['radius'],
            source_radius=source['radius'], nav_flags=flags, crouch=bool(flags & 64), spawn=False,
            topology_protected=True, locked=False, generated_guide=True,
            source_reason=source.get('reason', ''), editor_role=source.get('editor_role', 0),
            name=source.get('name', ''), facing=source.get('facing', [0, 0, 0]), way_guid=source['way_guid'],
            action_roles=protection['action_roles'].get(identity, []),
            action_properties=protection['action_properties'].get(identity, {})))
    result['nodes'].sort(key=lambda n: n['id'])
    result['edges'] = [dict(zip(('from', 'to'), p)) for p in sorted(after)]
    result['next_id'] = max(doc['next_id'], max(added, default=-1) + 1)
    return result, dict(added_node_ids=sorted(added), approach_node_ids=sorted(added&guides), added_directed_edges=len(after - before),
                       retained_node_ids=sorted(original), deleted_nodes=0,
                       native_generation=report['door_generation'], native_revalidation_required=True)
