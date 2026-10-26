"""Stage authenticated Omni-bot scripts, separating authoring registrations."""
import hashlib
import json
from pathlib import Path
import shutil

HERE = Path(__file__).resolve().parent


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def tokens(source):
    """Bounded GM lexer: offsets survive strings, escapes and both comments."""
    result, i, size = [], 0, len(source)
    while i < size:
        start = i
        if source[i].isspace():
            i += 1
            continue
        if source.startswith('//', i):
            end = source.find('\n', i + 2)
            i = size if end < 0 else end + 1
            continue
        if source.startswith('/*', i):
            end = source.find('*/', i + 2)
            if end < 0:
                raise ValueError('Unterminated GM comment')
            i = end + 2
            continue
        if source[i] in ('"', "'"):
            quote = source[i]
            i += 1
            while i < size:
                if source[i] == '\\':
                    i += 2
                elif source[i] == quote:
                    i += 1
                    break
                else:
                    i += 1
            else:
                raise ValueError('Unterminated GM string')
            result.append(('string', source[start + 1:i - 1], start, i))
        elif source[i].isalnum() or source[i] == '_':
            i += 1
            while i < size and (source[i].isalnum() or source[i] == '_'):
                i += 1
            result.append(('identifier', source[start:i], start, i))
        else:
            result.append(('punctuation', source[i], i, i + 1))
            i += 1
    return result


def command_spans(source):
    lexed, result = tokens(source), {}
    for i, token in enumerate(lexed):
        if token[1] != 'Commands' or token[0] != 'identifier':
            continue
        if i + 1 < len(lexed) and lexed[i + 1][1] == '[':
            level = 0
            for j in range(i + 1, len(lexed)):
                if lexed[j][0] != 'punctuation':
                    continue
                if lexed[j][1] == '[':
                    level += 1
                elif lexed[j][1] == ']':
                    level -= 1
                    if not level:
                        if (j + 1 < len(lexed) and lexed[j + 1][1] == '=' and
                                (j != i + 3 or lexed[i + 2][0] != 'string')):
                            raise ValueError('Unclassified dynamic command registration')
                        break
        following = lexed[i + 1:i + 5]
        if len(following) != 4 or [x[1] for x in following[::2]] != ['[', ']']:
            if len(following) >= 3 and following[0][1] == '.' and following[2][1] == '=':
                raise ValueError('Unclassified dotted command registration')
            continue
        if following[1][0] != 'string' and following[3][1] == '=':
            raise ValueError('Unclassified dynamic command registration')
        if following[1][0] != 'string' or following[3][1] != '=':
            continue
        name, stack, end = following[1][1], [], None
        for current in lexed[i + 5:]:
            value = current[1]
            if current[0] == 'punctuation':
                if value in '{([':
                    stack.append(value)
                elif value in '})]':
                    if not stack or stack.pop() != {'}': '{', ')': '(', ']': '['}[value]:
                        raise ValueError('Unbalanced command: ' + name)
                elif value == ';' and not stack:
                    end = current[3]
                    break
        if end is None or name in result:
            raise ValueError('Missing terminator or duplicate command: ' + name)
        result[name] = (token[2], end)
    return result


def classify_commands(payload, policy):
    """Fail closed when any script registry changes without an explicit review."""
    if policy.get('schema') != 2:
        raise ValueError('Expected exhaustive runtime command policy schema 2')
    public, editor = policy['public_commands'], policy['editor_commands']
    if set(public) != set(editor):
        raise ValueError('Command policy file sets differ')
    missing = (set(public) | set(policy['editor_files'])) - payload.keys()
    if missing:
        raise ValueError('Missing classified script: ' + ', '.join(sorted(missing)))
    result = {}
    for relative, data in sorted(payload.items()):
        if not relative.endswith('.gm'):
            continue
        text = data.decode('latin-1')
        commands = command_spans(text)
        if relative in policy['editor_files']:
            allowed, blocked = set(), set(commands)
        elif commands or relative in public:
            if relative not in public:
                raise ValueError('Unclassified GM command registry: ' + relative)
            allowed, blocked = set(public[relative]), set(editor[relative])
            if allowed & blocked or set(commands) != allowed | blocked:
                raise ValueError('GM command policy differs: ' + relative + ' ' +
                                 repr(sorted(set(commands) ^ (allowed | blocked))))
        else:
            continue
        result[relative] = {'public': sorted(allowed), 'editor': sorted(blocked),
                            'source_sha256': hashlib.sha256(data).hexdigest(),
                            'registrations': []}
        for name, (start, end) in commands.items():
            statement = text[start:end]
            pieces = tokens(statement)
            alias = None
            if len(pieces) == 10 and pieces[5][1] == 'Commands' and pieces[7][0] == 'string':
                alias = pieces[7][1]
                if name in allowed and alias not in allowed:
                    raise ValueError('Public command aliases an editor command: ' + name)
            result[relative]['registrations'].append({
                'name': name, 'category': 'public' if name in allowed else 'editor',
                'statement_sha256': hashlib.sha256(statement.encode('latin-1')).hexdigest(),
                'alias': alias})
    return result


def call_spans(source):
    """Return global call spans; strings/comments/member methods cannot match."""
    lexed = tokens(source)
    for i, token in enumerate(lexed):
        if token[0] != 'identifier' or i + 1 >= len(lexed) or lexed[i + 1][1] != '(':
            continue
        if i and lexed[i - 1][1] in ('.', ':'):
            continue
        stack, closing = [], None
        for j in range(i + 1, len(lexed)):
            current = lexed[j]
            if current[0] != 'punctuation':
                continue
            if current[1] in '([{':
                stack.append(current[1])
            elif current[1] in ')]}':
                if not stack or stack.pop() != {')': '(', ']': '[', '}': '{'}[current[1]]:
                    raise ValueError('Unbalanced GM function call')
                if not stack:
                    closing = j
                    break
        if closing is None:
            raise ValueError('Unterminated GM function call')
        yield token[1], lexed[i + 2:closing], token[2], lexed[closing][3], lexed, i, closing


def filter_diagnostics(source, relative, policy, navigation_maps):
    """Disable only reviewed Studio diagnostic calls, preserving mission code."""
    edits = []
    status_counts = {'SetCvar': 0, 'print': 0}
    diagnostic_prints = {name: 0 for name in policy['diagnostic_prints'].get(relative, [])}
    navfiles = {'et/nav/' + name + '_gt' + str(gt) + '.gm'
                for name in navigation_maps for gt in (2, 5)}
    for name, args, start, end, lexed, i, closing in call_spans(source):
        if name == 'GetCvar' and len(args) == 1 and args[0][0] == 'string':
            cvar = args[0][1]
            if cvar.startswith('omnibot_tce_') and cvar.endswith('_debug'):
                if cvar not in policy['diagnostic_cvars']:
                    raise ValueError('Unclassified Studio diagnostic cvar: ' + cvar)
                edits.append((start, end, '0', 'disabled_diagnostic_cvar', cvar))
        status = (name == 'SetCvar' and args and args[0][0] == 'string' and
                  args[0][1] == policy['editor_status_cvar'])
        loaded = (name == 'print' and args and args[0][0] == 'string' and
                  args[0][1].startswith(policy['editor_print_prefix']))
        diagnostic = (name == 'print' and args and args[0][0] == 'string' and
                      args[0][1].startswith('TCE2'))
        if diagnostic:
            if args[0][1] not in diagnostic_prints:
                raise ValueError('Unclassified navigation diagnostic print: ' + relative + ': ' + args[0][1])
            diagnostic_prints[args[0][1]] += 1
        if not (status or loaded or diagnostic):
            continue
        if (status or loaded) and relative not in navfiles:
            raise ValueError('Unclassified Studio status script: ' + relative)
        if (i == 0 or lexed[i - 1][1] not in ('{', ';', '}') or
                closing + 1 >= len(lexed) or lexed[closing + 1][1] != ';'):
            raise ValueError('Studio status call is not a standalone statement')
        if status and (len(args) != 3 or args[1][1] != ',' or not args[2][1].isdigit()):
            raise ValueError('Unexpected Studio status value')
        expected_print = policy['editor_print_prefix'] + Path(relative).stem.replace('_gt', ' GT') + ' loaded'
        if loaded and (len(args) != 1 or args[0][1] != expected_print):
            raise ValueError('Unexpected Studio loaded message')
        if status or loaded:
            status_counts[name] += 1
        edits.append((start, lexed[closing + 1][3], '/* Editor-only navigation diagnostic. */',
                      'removed_navigation_diagnostic' if diagnostic else 'removed_studio_status', args[0][1]))
    if relative in navfiles and status_counts != {'SetCvar': 1, 'print': 1}:
        raise ValueError('Expected one reviewed Studio status pair: ' + relative)
    if any(count != 1 for count in diagnostic_prints.values()):
        raise ValueError('Navigation diagnostic print policy differs: ' + relative)
    previous, chunks, audit = 0, [], []
    for start, end, replacement, kind, value in sorted(edits):
        if start < previous:
            raise ValueError('Overlapping diagnostic transformations')
        chunks.extend((source[previous:start], replacement))
        audit.append({'kind': kind, 'value': value,
                      'source_sha256': hashlib.sha256(source[start:end].encode('latin-1')).hexdigest(),
                      'replacement': replacement})
        previous = end
    chunks.append(source[previous:])
    return ''.join(chunks), audit


def runtime_payload(repo):
    """Read authenticated imported scripts, then the versioned TC overlays."""
    base = Path(repo) / 'vendor/omni-bot/runtime'
    source = json.loads((base / 'SOURCE.json').read_text(encoding='utf-8'))
    payload = {}
    for item in source['files']:
        path = base / item['path']
        if sha256(path) != item['sha256']:
            raise ValueError('Runtime source fingerprint differs: ' + item['path'])
        payload[item['path']] = path.read_bytes()
    for folder, prefix in (('tce2-global-scripts', 'global_scripts'), ('tce2-scripts', 'et/scripts')):
        overlay = Path(repo) / 'vendor/omni-bot' / folder
        for path in sorted(overlay.rglob('*.gm')):
            payload[prefix + '/' + path.relative_to(overlay).as_posix()] = path.read_bytes()
    return payload, source


def filter_commands(source, blocked):
    """Remove exact registration statements; preserve every retained byte."""
    before = command_spans(source)
    missing = set(blocked) - before.keys()
    if missing:
        raise ValueError('Authoring policy has unknown command names: ' + ', '.join(sorted(missing)))
    cuts = sorted(before[name] for name in blocked)
    previous, chunks = 0, []
    for start, end in cuts:
        if start < previous:
            raise ValueError('Overlapping command registrations')
        chunks.extend((source[previous:start], '/* Authoring command is available in the separate editor package. */'))
        previous = end
    chunks.append(source[previous:])
    filtered = ''.join(chunks)
    after = command_spans(filtered)
    if set(after) != set(before) - set(blocked):
        raise ValueError('Unexpected command-set change')
    for name, (start, end) in after.items():
        a, b = before[name]
        if filtered[start:end] != source[a:b]:
            raise ValueError('Gameplay command changed: ' + name)
    return filtered, {'removed': sorted(blocked), 'retained': sorted(after)}


def prepare_runtime(repo, variant):
    """Build the deterministic script/data closure without touching an output."""
    repo = Path(repo).resolve()
    if variant not in ('game', 'navstudio'):
        raise ValueError('Unknown runtime variant')
    policy = json.loads((HERE / 'runtime-policy.json').read_text(encoding='utf-8'))
    payload, source = runtime_payload(repo)
    missing = set(policy['required_files']) - payload.keys()
    if missing:
        raise ValueError('Incomplete script closure: ' + ', '.join(sorted(missing)))
    registry = classify_commands(payload, policy)
    filtered, diagnostics = {}, {}
    if variant == 'game':
        for relative in policy['editor_files']:
            if relative not in payload:
                raise ValueError('Missing classified authoring helper: ' + relative)
            del payload[relative]
        for relative, blocked in policy['editor_commands'].items():
            data = payload[relative]
            # Decode latin-1 solely for byte-preserving offsets; no encoding conversion.
            text, filtered[relative] = filter_commands(data.decode('latin-1'), blocked)
            payload[relative] = text.encode('latin-1')
        for relative, data in payload.items():
            if relative.endswith('.gm'):
                text, changes = filter_diagnostics(data.decode('latin-1'), relative, policy,
                                                   source['navigation_scope']['maps'])
                if changes:
                    diagnostics[relative] = changes
                    payload[relative] = text.encode('latin-1')
    return payload, source, registry, filtered, diagnostics


def stage_runtime(repo, destination, variant):
    """Shared release/Windows-builder API; destination is the bot base folder."""
    repo, destination = Path(repo).resolve(), Path(destination).resolve()
    base = repo / 'vendor/omni-bot/runtime'
    payload, source, registry, filtered, diagnostics = prepare_runtime(repo, variant)
    destination.mkdir(parents=True, exist_ok=True)
    unexpected = {p.relative_to(destination).as_posix() for p in destination.rglob('*.gm')} - payload.keys()
    if unexpected:
        raise ValueError('Stale/unclassified runtime scripts in output: ' + ', '.join(sorted(unexpected)))
    files, mod_files = [], []
    for relative, data in sorted(payload.items()):
        if relative.startswith('release_mapscripts/'):
            mod_relative = 'mapscripts/' + Path(relative).name
            path = destination.parent / mod_relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            mod_files.append({'path': mod_relative, 'sha256': hashlib.sha256(data).hexdigest()})
            continue
        path = destination / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        files.append({'path': relative, 'sha256': hashlib.sha256(data).hexdigest()})
    for relative in ('SOURCE.json', 'IMPORT-NOTICE.txt'):
        shutil.copy2(base / relative, destination / ('RUNTIME-' + relative))
    shutil.copy2(repo / 'vendor/omni-bot/IMPORT-NOTICE.txt', destination / 'IMPORT-NOTICE.txt')
    (destination / 'config').mkdir(exist_ok=True)
    shutil.copy2(HERE / 'omni-bot.cfg', destination / 'config/omni-bot.cfg')
    activation = destination.parent / 'omnibot-release.cfg'
    activation.write_text(
        '// Execute before map startup. Choose bot counts in omni-bot/config/omni-bot.cfg.\n'
        'set omnibot_enable 1\nset omnibot_path ""\n'
        'set g_mapScriptDirectory "mapscripts"\n', encoding='utf-8', newline='\n')
    mod_files.append({'path': activation.name, 'sha256': sha256(activation)})
    manifest = {'schema': 2, 'variant': variant, 'files': files,
                'mod_files': mod_files, 'commands': filtered,
                'command_registry': registry, 'diagnostics': diagnostics,
                'policy_sha256': sha256(HERE / 'runtime-policy.json'),
                'navigation_scope': source.get('navigation_scope'),
                'source_manifest_sha256': sha256(base / 'SOURCE.json')}
    (destination / 'runtime-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8', newline='\n')
    return manifest
