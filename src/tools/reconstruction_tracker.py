"""Persistent per-binary function ledger and generated Markdown checklists.

sync merges Ghidra/ELF inventories without resetting existing progress.
check checks coverage, evidence and generated lists without writing files.
set updates one stable PROGRAM:ADDRESS record, then renders the lists.
"""
import argparse
from collections import Counter
import json
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
ROOT = SRC.parent
BASE = SRC / 'reconstruction'
LEDGER = BASE / 'functions.json'
STATUSES = ('pending', 'in_progress', 'verified', 'excluded')


def inventory():
    rows = {}
    for path in sorted((BASE / 'inventory/ghidra').glob('*.json')):
        snapshot = json.loads(path.read_text(encoding='utf-8'))
        program = snapshot['program']
        for function in snapshot['functions']:
            address = function['entry_point'].lower()
            key = program + ':' + address
            if key in rows:
                raise ValueError('Duplicate Ghidra address: ' + key)
            rows[key] = dict(id=key, program=program, address=address,
                             name=function['name'], aliases=[], origins=['ghidra'])
        if program.endswith('.so'):
            # st_value is relative to the ELF image; do not confuse with loaded VA.
            base = int(snapshot['metadata']['imageBase'], 16)
            module = program.split('.')[0]
            elf = json.loads((BASE / 'inventory' / (module + '.json')).read_text())
            for function in elf['symbols']:
                if function['kind'] != 'function':
                    continue
                address = f"{int(function['address'], 16) + base:08x}"
                key = program + ':' + address
                if key not in rows:
                    rows[key] = dict(id=key, program=program, address=address,
                                     name=function['name'], aliases=[], origins=[])
                row = rows[key]
                if row['name'] != function['name']:
                    if row['name'].startswith(('FUN_', 'thunk_FUN_')):
                        row['aliases'].append(row['name'])
                        row['name'] = function['name']
                    else:
                        row['aliases'].append(function['name'])
                row['origins'] = sorted(set(row['origins'] + ['elf_symbol']))
                row['aliases'] = sorted(set(row['aliases']))
    if len({r['program'] for r in rows.values()}) != 6:
        raise ValueError('Expected inventories for all six binaries')
    return rows


def merge(old, found):
    records = {r['id']: r for r in old['functions']}
    if len(records) != len(old['functions']):
        raise ValueError('Duplicate ledger IDs')
    for key, discovered in found.items():
        if key not in records:
            records[key] = dict(discovered, status='pending', source=[], evidence=[],
                                validation='', integration='pending', notes='')
        else:
            row = records[key]
            # Keep manually established names and every status/evidence field.
            aliases = set(row['aliases'] + discovered['aliases'])
            if row['name'] != discovered['name']:
                if row['name'].startswith(('FUN_', 'thunk_FUN_')):
                    aliases.add(row['name']); row['name'] = discovered['name']
                else:
                    aliases.add(discovered['name'])
            row['aliases'] = sorted(aliases - {row['name']})
            row['origins'] = sorted(set(row['origins'] + discovered['origins']))
    return dict(schema=1, functions=sorted(records.values(), key=lambda r: r['id']))


def validate(data, found):
    records = {r['id']: r for r in data['functions']}
    if len(records) != len(data['functions']):
        raise ValueError('Duplicate ledger IDs')
    if set(found) - set(records):
        raise ValueError('Inventory entries missing from ledger; run sync')
    for row in records.values():
        if row['status'] not in STATUSES:
            raise ValueError('Invalid status: ' + row['id'])
        if row['integration'] not in ('pending', 'integrated', 'not_applicable'):
            raise ValueError('Invalid integration: ' + row['id'])
        if row['status'] == 'verified':
            if not all((row['source'], row['evidence'], row['validation'])):
                raise ValueError('Verified entry lacks proof: ' + row['id'])
        if row['status'] == 'excluded' and not (row['notes'] and row['evidence']):
            raise ValueError('Exclusion lacks reason/evidence: ' + row['id'])
        for path in row['source'] + row['evidence']:
            resolved = (ROOT / path).resolve()
            if not resolved.is_relative_to(SRC) or not resolved.is_file():
                raise ValueError('Missing/non-source evidence path: ' + path)


def render(data):
    outputs = {}
    overview = [
        '# Funktionscheckliste TC:E', '',
        'Generiert aus `functions.json`; Status dort bzw. mit dem Tracker aendern.', '',
        '`[x]` = rekonstruiert und verhaltensgeprueft. Einbindung separat beachten.',
        '`[ ]` = offen/in Arbeit oder begruendet ausgeschlossen. Keine automatische',
        'Uebertragung des Status zwischen Modulen oder Plattformen.', '',
        'Umfang: alle intern von Ghidra erfassten Funktionen der sechs Module,',
        'ergaenzt um alle definierten ELF-Funktionssymbole. Gleiche Einstiegspunkte',
        'mit Aliasnamen bilden einen Eintrag. Externe Bibliotheksimporte haben',
        'keinen Funktionskoerper in diesen Binaries und werden nicht portiert.',
        'Weitere durch Analyse entdeckte Funktionen werden beim Sync hinzugefuegt.', '',
        '| Modul | Erfasst | Offen | In Arbeit | Verifiziert | Ausgeschlossen |',
        '|---|---:|---:|---:|---:|---:|'
    ]
    for program in sorted({r['program'] for r in data['functions']}):
        rows = [r for r in data['functions'] if r['program'] == program]
        counts = Counter(r['status'] for r in rows)
        overview.append(f'| [{program}](checklists/{program}.md) | {len(rows)} | ' +
                        ' | '.join(str(counts[s]) for s in STATUSES) + ' |')
        lines = [f'# {program}', '',
                 f'{len(rows)} Einstiegspunkte; {counts["verified"]} verifiziert.', '',
                 'Haken: Funktion verifiziert. Integration wird separat angegeben.', '']
        for r in rows:
            mark = 'x' if r['status'] == 'verified' else ' '
            aliases = ('; Alias: ' + ', '.join(r['aliases'])) if r['aliases'] else ''
            line = f'- [{mark}] `{r["name"]}` @ `{r["address"]}` — {r["status"]}; Integration: {r["integration"]}{aliases}'
            if r['source']:
                line += '; Code: ' + ', '.join(f'`{p}`' for p in r['source'])
            lines.append(line)
        outputs[BASE / 'checklists' / (program + '.md')] = '\n'.join(lines) + '\n'
    overview += ['', '## Aktive Arbeit', '']
    active = [r for r in data['functions'] if r['status'] == 'in_progress']
    overview += [f'- `{r["id"]}` — {r["name"]}: {r["notes"]}' for r in active] or ['Keine aktive Funktion. Naechsten Schritt in [WORKLOG.md](WORKLOG.md) lesen.']
    overview += ['', 'Die Zaehler messen Eintraege, nicht den Fertigstellungsgrad des Mods.',
                 'Belege, Tests und Integrationsstatus stehen in `functions.json`.',
                 'Gesamtstand: [STATUS.md](STATUS.md). Arbeitsjournal: [WORKLOG.md](WORKLOG.md).', '']
    outputs[BASE / 'FUNCTIONS.md'] = '\n'.join(overview)
    return outputs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['sync', 'check', 'set'])
    parser.add_argument('id', nargs='?')
    parser.add_argument('--status', choices=STATUSES)
    parser.add_argument('--source', action='append')
    parser.add_argument('--evidence', action='append')
    parser.add_argument('--validation')
    parser.add_argument('--notes')
    parser.add_argument('--integration', choices=['pending', 'integrated', 'not_applicable'])
    args = parser.parse_args()
    found = inventory()
    old = json.loads(LEDGER.read_text(encoding='utf-8')) if LEDGER.exists() else dict(schema=1, functions=[])
    data = old if args.command == 'check' else merge(old, found)
    if args.command == 'set':
        row = next((r for r in data['functions'] if r['id'] == args.id), None)
        if row is None:
            parser.error('Unknown ID: ' + str(args.id))
        for field in ('status', 'source', 'evidence', 'validation', 'notes', 'integration'):
            if getattr(args, field) is not None:
                row[field] = getattr(args, field)
    validate(data, found)
    outputs = render(data)
    if args.command == 'check':
        for path, content in outputs.items():
            if not path.exists() or path.read_text(encoding='utf-8') != content:
                raise ValueError('Stale checklist: ' + str(path))
    else:
        LEDGER.write_text(json.dumps(data, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
        for path, content in outputs.items():
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding='utf-8')
    print(f'{len(data["functions"])} functions; ' + str(dict(Counter(r['status'] for r in data['functions']))))


if __name__ == '__main__':
    main()
