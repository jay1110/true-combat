"""Read-only objective evidence from native Omni-bot status replies.

A loaded goal, an owner change, a missing bomb or a GM log message does not
prove that a particular bot planted/defused it. Only a positive native counter
delta on the same actor/goal is recorded here. First samples are baselines.
"""
import math
import re


INTEGER = re.compile(r'-?\d+\Z')
BOT = re.compile(r'\bTCEOBJECTIVE_BOT\s+(\d+)\s+(.+)')
GOAL = re.compile(r'\bTCEOBJECTIVE_STATUS\s+(.+?)\s+entity\s+(-?\d+)\s+(.+)')
OBSERVER_FIELDS = ('plants', 'defuses', 'plant_time', 'defuse_time',
                   'plant_bomb', 'defuse_bomb', 'plant_objective')


def fields(text):
    tokens = text.split()
    if len(tokens) % 2:
        raise ValueError('status fields are not name/value pairs')
    result = {}
    for key, value in zip(tokens[::2], tokens[1::2]):
        if key in result:
            raise ValueError('duplicate native field ' + key)
        result[key] = int(value) if INTEGER.fullmatch(value) else None
    return result


def parse_reply(reply):
    result = dict(bots={}, goals={}, warnings=[])
    ambiguous = set()
    clean = re.sub(r'\^[0-9]', '', str(reply))
    for line in clean.splitlines():
        bot, goal = BOT.search(line), GOAL.search(line)
        if not bot and not goal:
            continue
        try:
            if bot:
                row = fields(bot[2]); key = int(bot[1]); kind = 'bots'
                row['slot'] = key
            else:
                row = fields(goal[3]); row.update(name=goal[1].strip(), entity=int(goal[2]))
                key = (row['name'], row['entity']); kind = 'goals'
            if (kind, key) in ambiguous:
                continue
            if key in result[kind] and result[kind][key] != row:
                del result[kind][key]
                ambiguous.add((kind, key))
                raise ValueError('conflicting duplicate native status')
            result[kind][key] = row
        except ValueError as exc:
            result['warnings'].append(str(exc) + ': ' + line)
    return result


def summarize(samples):
    """Compare consecutive native snapshots; never infer from bomb existence."""
    events, resets, warnings = [], [], []
    previous = None
    observer_available = False
    for sample in samples or ():
        timestamp = sample.get('time')
        if not isinstance(timestamp, (int, float)) or not math.isfinite(timestamp):
            warnings.append('Objective snapshot has no finite observation time.')
            previous = None
            continue
        current = parse_reply(sample.get('reply', ''))
        current['time'] = timestamp
        warnings.extend(current['warnings'])
        observer_available |= any(all(row.get(key) is not None for key in OBSERVER_FIELDS)
                                  for row in current['bots'].values())
        if previous and timestamp <= previous['time']:
            warnings.append('Objective snapshots are not in increasing observation order.')
            previous = current
            continue
        if previous:
            for slot, row in current['bots'].items():
                before = previous['bots'].get(slot)
                if not before or row.get('team') not in (1, 2) or row.get('team') != before.get('team'):
                    continue
                for action, counter, prefix in (('PLANTED', 'plants', 'plant'),
                                                ('DEFUSED', 'defuses', 'defuse')):
                    old_count, new_count = before.get(counter), row.get(counter)
                    if old_count is None or new_count is None or old_count < 0 or new_count < 0:
                        continue
                    if new_count < old_count:
                        resets.append(dict(actor=slot, counter=counter, time=timestamp))
                        continue
                    if new_count == old_count:
                        continue
                    native_time, bomb = row.get(prefix + '_time'), row.get(prefix + '_bomb')
                    old_time = before.get(prefix + '_time')
                    objective = row.get('plant_objective') if action == 'PLANTED' else None
                    # Each native event records time and identity in the same
                    # authoritative branch as its count. Null legacy stats and
                    # stale/malformed identity receipts cannot confirm an event.
                    if (native_time is None or native_time <= 0 or bomb is None or bomb < 0
                            or old_time is None or native_time <= old_time
                            or action == 'PLANTED' and (objective is None or objective < 0)):
                        warnings.append(f'{action} counter advanced without a fresh native action receipt for actor {slot}.')
                        continue
                    delta = new_count - old_count
                    events.append(dict(action=action, actor=slot, team=row.get('team'),
                        bomb=bomb, objective=objective, native_time=native_time,
                        count_before=old_count, count_after=new_count, count_delta=delta,
                        last_action_only=delta > 1, observed_after=previous['time'],
                        observed_by=timestamp, evidence='authoritative_native_actor_counter'))
            for key, row in current['goals'].items():
                before = previous['goals'].get(key)
                old_count, new_count = (before or {}).get('captures'), row.get('captures')
                if old_count is None or new_count is None or old_count < 0 or new_count < 0:
                    continue
                if new_count < old_count:
                    resets.append(dict(goal=row['name'], entity=row['entity'], counter='captures', time=timestamp))
                elif new_count > old_count:
                    events.append(dict(action='CAPTURED', goal=row['name'], entity=row['entity'],
                        count_before=old_count, count_after=new_count, count_delta=new_count-old_count,
                        observed_after=previous['time'], observed_by=timestamp, actor=None,
                        evidence='authoritative_native_flag_capture_counter'))
        previous = current
    if not observer_available:
        warnings.append('Native plant/defuse observation fields were unavailable; GM messages remain unconfirmed.')
    return dict(completions=events, observer_available=observer_available,
                counter_resets=resets, warnings=warnings, mission_behavior_complete=False)
