"""Inventory mod compatibility risks without changing or executing target code.

Function extraction is conservative C/C++ source indexing, not a parser/parity
proof. File-wide guard inventories remain available when extraction is ambiguous.
Class hints require review: A=OS/API, B=ABI/numeric compatibility,
C=compiler/architecture/build contract, D=developer/authoring features.
"""

import argparse
import bisect
import hashlib
import json
from pathlib import Path
import re


LEXICAL = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)
FUNCTION = re.compile(
    r'(?m)^[ \t]*[^\n;{}#]*?\b(?P<name>[A-Za-z_]\w*(?:::[~A-Za-z_]\w*)*)'
    r'[ \t]*\([^;{}]*?\)[ \t]*(?:const[ \t]*)?(?:\n[ \t]*)?\{')
DIRECTIVE = re.compile(r'^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)')
PLATFORM = re.compile(r'\b(?:__linux__|_WIN32|WIN32|__APPLE__|__ANDROID__|__unix__|__EMSCRIPTEN__)\b')
ARCH = re.compile(r'\b(?:_MSC_VER|_M_IX86|_M_X64|__i386__|__x86_64__|__aarch64__|__arm__|Q3_VM)\b')
DEV = re.compile(r'\b(?:FEATURE_NAVSTUDIO|NAVSTUDIO|DEBUG|_DEBUG|BUILD_TESTING)\b')
OS_API = re.compile(r'\b(?:CreateFile|GetModuleHandle|VirtualAlloc|ReadProcessMemory|GetProcAddress|LoadLibrary|pthread_|dlopen|dlsym|mmap|clock_gettime|GetTickCount)\w*')
MATH = re.compile(r'long\s+double|__control87|_controlfp|x87|TCE_(?:Trigger|Fixed|Rotate|Finite)|precision', re.I)
EXCLUDED = {'vendor', 'tests', 'tools', 'reconstruction', 'build', 'Debug', 'Release', '.git'}


def mask(text, strings=True):
    def replace(match):
        value = match.group()
        if not strings and value[:1] in ('"', "'"):
            return value
        return ''.join('\n' if char == '\n' else ' ' for char in value)
    return LEXICAL.sub(replace, text)


def classify(condition, context):
    if DEV.search(condition):
        return 'D'
    if MATH.search(context):
        return 'B'
    if OS_API.search(context):
        return 'A'
    if ARCH.search(condition) or PLATFORM.search(condition) or 'FEATURE_' in condition:
        return 'C'
    return None


def normalized_hash(source):
    tokens = re.findall(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|\w+|[^\s]', mask(source, False))
    return hashlib.sha256('\x1f'.join(tokens).encode()).hexdigest()


def scan(path, root):
    raw = path.read_bytes()
    text = raw.decode('utf-8-sig')
    clean = mask(text)
    lines = clean.splitlines(keepends=True)
    offsets, scopes, guards, stack = [], [], [], []
    offset = 0
    for line_number, line in enumerate(lines, 1):
        offsets.append(offset)
        scopes.append(tuple(stack))
        match = DIRECTIVE.match(line)
        if match:
            kind, condition = match.groups()
            if kind in ('if', 'ifdef', 'ifndef'):
                stack.append(condition.strip())
            elif kind in ('else', 'elif') and stack:
                stack[-1] = (kind + ' ' + condition).strip()
            elif kind == 'endif' and stack:
                stack.pop()
            if kind in ('if', 'ifdef', 'ifndef', 'elif'):
                context = clean[max(0, offset - 200):offset + 1400]
                guards.append({'line': line_number, 'condition': condition.strip(),
                               'class_hint': classify(condition, context), 'reviewed': False})
        offset += len(line)
    functions = []
    for match in FUNCTION.finditer(clean):
        name = match.group('name')
        if name in ('if', 'for', 'while', 'switch', 'catch', 'sizeof'):
            continue
        opening = match.end() - 1
        depth, end = 1, opening + 1
        while end < len(clean) and depth:
            depth += (clean[end] == '{') - (clean[end] == '}')
            end += 1
        if depth:
            continue
        index = max(0, bisect.bisect_right(offsets, match.start()) - 1)
        body = clean[match.start():end]
        scope = '\n'.join(scopes[index])
        reasons = {}
        if PLATFORM.search(body + scope):
            reasons['platform_guard'] = 3
        if re.search(r'\blong\s+double\b', body):
            reasons['long_double'] = 4
        if re.search(r'\bvolatile\b', body):
            reasons['volatile'] = 4
        if re.search(r'\bunion\b[\s\S]*\b(?:float|double)\b|reinterpret_cast|\*\s*\(\s*(?:float|double|uint32_t|unsigned\s+int)\s*\*', body):
            reasons['bit_reinterpretation_candidate'] = 4
        if re.search(r'render|model|math|damage|trajectory|effect|entity|weapon|particle|view|camera', path.name + ' ' + name, re.I):
            reasons['gameplay_or_render_math'] = 3
        if OS_API.search(body) or re.search(r'\b(?:trap_|syscall|dllEntry|vmMain)', body) or re.search(r'compat|precision|wrap|abi', path.name, re.I):
            reasons['abi_or_wrapper'] = 2
        functions.append({'name': name, 'line': index + 1, 'end_line': text.count('\n', 0, end) + 1,
                          'score': sum(reasons.values()), 'reasons': reasons,
                          'normalized_sha256': normalized_hash(text[match.start():end])})
    return {'path': path.relative_to(root).as_posix(), 'sha256': hashlib.sha256(raw).hexdigest(),
            'utf8': True, 'crlf': raw.count(b'\r\n'), 'guards': guards, 'functions': functions}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root = args.root.resolve()
    files, errors = [], []
    for source in (root / 'src', root / 'compat', root / 'platform'):
        if not source.is_dir():
            continue
        for path in sorted(source.rglob('*')):
            if path.suffix not in ('.c', '.cpp', '.h', '.hpp', '.inl') or any(part in EXCLUDED for part in path.relative_to(source).parts):
                continue
            try:
                files.append(scan(path, root))
            except UnicodeError as error:
                errors.append({'path': path.relative_to(root).as_posix(), 'error': str(error)})
    ranked = sorted((dict(function, path=file['path']) for file in files for function in file['functions']),
                    key=lambda item: (-item['score'], item['path'], item['line']))
    report = {'schema': 1, 'classifications': {'A': 'OS/API abstraction', 'B': 'ABI/numeric compatibility',
              'C': 'compiler/architecture/build contract', 'D': 'developer/authoring features'},
              'limits': 'Heuristic inventory; hints are not reviewed decisions. Conservative source indexing is not complete AST coverage or parity proof.',
              'summary': {'files': len(files), 'functions_indexed': len(ranked),
                          'guard_directives': sum(len(file['guards']) for file in files),
                          'score_at_least_9': sum(item['score'] >= 9 for item in ranked), 'decode_errors': len(errors)},
              'files': files, 'ranked': ranked, 'errors': errors}
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / 'risk-inventory.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    rows = ['# Compatibility risk inventory', '', report['limits'], '', str(report['summary']), '',
            '| Score | Function | Source | Signals |', '|---:|---|---|---|']
    for item in ranked[:60]:
        rows.append(f"| {item['score']} | {item['name']} | {item['path']}:{item['line']} | {', '.join(item['reasons'])} |")
    (args.out / 'risk-summary.md').write_text('\n'.join(rows) + '\n', encoding='utf-8')
    print(json.dumps(report['summary']))
    if errors:
        print('Some source files require encoding review; see errors in report.')


if __name__ == '__main__':
    main()
