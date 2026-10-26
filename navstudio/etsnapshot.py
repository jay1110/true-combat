"""Join original ET's public engine observations to BSP inline geometry.

The engine API does not expose brush model indices or contents. A match must
be unique in local bounds and either pose or name. Unmatched models retain an
explicit provisional BSP spawn policy; final native hull traces decide them.
"""
import hashlib
import math


def close(a, b, tolerance=2):
    return (isinstance(a,(list,tuple)) and isinstance(b,(list,tuple)) and len(a)==len(b)
            and all(type(x) in (float,int) and type(y) in (float,int) and
                    math.isfinite(x) and math.isfinite(y) and abs(x-y)<=tolerance for x,y in zip(a,b)))


def join_models(models, entities, snapshot, mask):
    from tce_bsp_navmesh import inline_instances
    observations=snapshot.get("engine_entities", [])
    if not isinstance(observations,list): raise ValueError("ET engine entity observations missing")
    joined, ledger, used = [], [], set()
    provisional=inline_instances(models,entities,mask,None)
    for fallback in provisional:
        model=fallback["model"]
        bsp_id=fallback.get("entity")
        if bsp_id is None:
            ledger.append(dict(model=model,status="unreferenced_bsp_model")); continue
        entity=entities[bsp_id]; bounds=models[model]
        names={entity.get(key,"") for key in ("targetname","scriptname","name")}-{ "" }
        matches=[]
        for observed in observations:
            if not isinstance(observed,dict) or observed.get("entity") in used: continue
            if not close(observed.get("local_mins"),bounds[:3]) or not close(observed.get("local_maxs"),bounds[3:6]): continue
            name_match=observed.get("name") in names
            pose_match=(close(observed.get("origin"),fallback["origin"]) and
                        close(observed.get("angles"),fallback["angles"]))
            if name_match or pose_match: matches.append(observed)
        if len(matches)==1:
            state=matches[0]; used.add(state["entity"])
            observed=state.get("collision_observed") is True
            enabled=observed or bool(fallback["enabled"])
            joined.append(dict(entity=state["entity"],modelindex=model,linked=enabled,
                contents=mask if enabled else 0,origin=state["origin"],angles=state["angles"],
                classname=entity.get("classname",""),state_source=("et_engine_unique_bsp_join" if observed
                    else "et_engine_pose_join_provisional_bsp_collision_probe_missed"),
                collision_state_observed=observed,native_body_validation_required=True))
            ledger.append(dict(model=model,bsp_entity=bsp_id,engine_entity=state["entity"],
                               status="unique_local_bounds_and_pose_or_name",collision_observed=observed))
        else:
            # Provisional geometry must not disappear merely because the old
            # ET API lacks an authoritative model-id enumeration.
            joined.append(dict(entity=-bsp_id-1,modelindex=model,linked=bool(fallback["enabled"]),
                contents=mask if fallback["enabled"] else 0,origin=fallback["origin"],angles=fallback["angles"],
                classname=entity.get("classname",""),state_source="provisional_bsp_spawn_policy_et_api_unmatched",
                collision_state_observed=False,native_body_validation_required=True))
            ledger.append(dict(model=model,bsp_entity=bsp_id,status="ambiguous" if matches else "unmatched",
                               matching_engine_entities=[m["entity"] for m in matches],
                               provisional_enabled=bool(fallback["enabled"])))
    return {**snapshot,"models":joined,"et_model_join":dict(schema=1,models=ledger,
        matched=sum(r.get("status")=="unique_local_bounds_and_pose_or_name" for r in ledger),
        provisional=sum(not m.get("collision_state_observed") for m in joined),
        complete_authoritative_collision=False,native_body_validation_required=True)}


def join_bsp(data, snapshot):
    from tce_bsp_navmesh import read_lumps,parse_entities,unpack_lump,source_flags
    lumps=read_lumps(data); lump=lumps[0]
    entities=parse_entities(data[lump["offset"]:lump["offset"]+lump["length"]])
    models=unpack_lump(data,lumps,7,"<6f4i")
    flags,_=source_flags(); mask=flags["CONTENTS_SOLID"]|flags["CONTENTS_PLAYERCLIP"]
    value=join_models(models,entities,snapshot,mask)
    value["et_model_join"]["bsp_sha256"]=hashlib.sha256(data).hexdigest()
    return value
