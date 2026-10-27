"""Checked adapter for explicitly bound normal-CGame read-only pose inputs.

These are supplied frame/transform inputs, not measured skinned vertices or tags.
No handle is resolved from a server registry, guessed by number, or substituted.
"""
from collections import Counter
import math
import pose_eval_fixtures as o


def vector(value, count, name):
    o.require(isinstance(value, list) and len(value) == count and
              all(isinstance(x, (int, float)) and math.isfinite(x) for x in value), name)
    return value


def matrix(value, name):
    flat = vector(value, 9, name)
    return [flat[index:index + 3] for index in (0, 3, 6)]


def fraction(value, name):
    o.require(isinstance(value, (int, float)) and math.isfinite(value) and 0 <= value <= 1, name)
    return value


def sample(row, assets, label, body_path, head_path):
    binding = row.get('cgame_character_binding', {})
    o.require(binding.get('selector_replayed') is True and binding.get('draw_eligible') is True,
              'missing coherent draw-eligible CGame selector binding')
    for name in ('carrier_override', 'corpse', 'disguised', 'undressed_corpse', 'special_vc100_model'):
        o.require(binding.get(name) is False, 'unsupported special CGame selection: ' + name)
    player = binding.get('player', {})
    animation = binding.get('animation', {})
    selected = player.get('character', {})
    o.require(selected.get('bounded_storage') is True and selected.get('pointer', 0) > 0 and
              player.get('pointer') == selected['pointer'] == animation.get('pointer') and
              animation.get('character') == selected and player.get('source') == 'clientInfo.character' and
              animation.get('source') == 'clientInfo.character' and
              binding.get('clientInfo', {}).get('character') == selected['pointer'],
              'unbound or divergent CGame player/animation character')
    character_path = selected.get('character_file', '')
    o.require(bool(character_path), 'CGame character file missing')
    character = assets.character(character_path)
    o.require(character['mesh'] == body_path and character['hudhead'] == head_path,
              'CGame character resolves a different body/head asset')
    body, head = row['render']['body'], row['render']['head']
    observed = binding.get('observed_handles', {})
    for key, ref, field, expected_name in (('body', body, 'mesh', 'expected_body'),
                                           ('head', head, 'hudhead', 'expected_head')):
        o.require(binding.get(key + '_handle_matches') is True and
                  ref['hModel'] > 0 and ref['hModel'] == selected.get(field) == observed.get(key) and
                  ref['hModel'] == binding.get(expected_name, {}).get('hModel') and
                  binding[expected_name].get('source') == 'character.' + field,
                  'CGame ' + key + ' handle/asset mismatch')
    o.require(binding.get('head_expected_submitted') is True, 'CGame head submission not eligible')
    # Resolve current handles from the actual selected character's animation
    # group and each recorded channel's metadata. Old handles must resolve by
    # the same checked mapping; unsupported cross-model transitions fail closed.
    mapping = {}
    for channel, ref_field in (('legs', 'frameModel'), ('torso', 'torsoFrameModel')):
        metadata = row['lerp'][channel]['animationMetadata']
        name = metadata['name'].lower()
        o.require(name in character['animations'] and metadata['mdxFile'] == body[ref_field],
                  'CGame animation name/handle lacks a selected-character binding')
        handle, path = metadata['mdxFile'], character['animations'][name]
        o.require(handle > 0 and (handle not in mapping or mapping[handle] == path),
                  'CGame animation handle resolves ambiguously')
        mapping[handle] = path
    handles = [body[key] for key in ('frameModel', 'oldframeModel', 'torsoFrameModel', 'oldTorsoFrameModel')]
    o.require(all(handle in mapping for handle in handles), 'unbound old CGame animation handle')
    frames = [body[key] for key in ('frame', 'oldframe', 'torsoFrame', 'oldTorsoFrame')]
    o.require(all(isinstance(frame, int) and frame >= 0 for frame in frames), 'invalid CGame frame')
    paths = [mapping[handle] for handle in handles]
    o.require(all(frame < assets.skeleton(path).frames for path, frame in zip(paths, frames)),
              'CGame frame exceeds bound MDX asset')
    return {'name': label, 'mesh': character['mesh'], 'models': paths, 'frames': frames,
            'back': [fraction(body['backlerp'], 'legs backlerp'),
                     fraction(body['torsoBacklerp'], 'torso backlerp')],
            'origin': vector(body['origin'], 3, 'body origin'), 'axis': matrix(body['axis'], 'body axis'),
            'torso': matrix(body['torsoAxis'], 'torso axis'),
            'head_origin': vector(head['origin'], 3, 'head origin'), 'head_axis': matrix(head['axis'], 'head axis'),
            'face': [head['frame'], head['oldframe'], fraction(head['backlerp'], 'head backlerp')],
            'bound_readonly': True, 'character': character_path}


def samples(document, assets, label, body_path, head_path):
    result, summary = [], []
    o.require(bool(document.get('cgame_sha256')) and bool(document.get('qagame_sha256')),
              'normal capture module hashes missing')
    for phase in document['phases']:
        rows = phase['consistent_samples']
        for index, row in enumerate(rows):
            result.append(sample(row, assets, label + ':' + phase['label'] + ':' + str(index), body_path, head_path))
        summary.append({'phase': phase['label'], 'count': len(rows),
                        'server_ladder_count': sum(bool(row['server_ps']['pm_flags'] & 4) for row in rows),
                        'predicted_ladder_count': sum(bool(row['predicted_ps']['pm_flags'] & 4) for row in rows),
                        'legs_animation_counts': dict(Counter(row['lerp']['legs']['animationMetadata']['name'] for row in rows))})
    o.require(bool(result), 'normal capture has no coherent rows')
    return result, summary
