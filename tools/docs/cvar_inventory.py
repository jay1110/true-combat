#!/usr/bin/env python3
"""Source-derived ET mod cvar declarations and reachable registration history.

This is a lexical inventory, not a compiler or an effective runtime cvar dump.
It preserves guards and repeated registrations, including engine-owned mirrors.
No engine checkout, original binaries or third-party Python modules are needed.
"""
from __future__ import annotations
import argparse
import ast
import csv
import io
import json
from pathlib import Path
import re
import subprocess
from collections import defaultdict
from decimal import Decimal, InvalidOperation

ROOT = Path(__file__).resolve().parents[2]
STRING = r'"(?:\\.|[^"\\])*"'
SOURCE_SUFFIXES = ('.c', '.cpp', '.h', '.inc')
TABLE = re.compile(r'\bcvarTable_t\s+([A-Za-z_]\w*)\s*\[\s*\]\s*=\s*\{')
CALL = re.compile(r'\btrap_Cvar_Register\s*\(')
FLAGS = {'CVAR_ARCHIVE':1, 'CVAR_USERINFO':2, 'CVAR_SERVERINFO':4,
         'CVAR_SYSTEMINFO':8, 'CVAR_INIT':16, 'CVAR_LATCH':32, 'CVAR_ROM':64,
         'CVAR_USER_CREATED':128, 'CVAR_TEMP':256, 'CVAR_CHEAT':512,
         'CVAR_NORESTART':1024, 'CVAR_WOLFINFO':2048, 'CVAR_UNSAFE':4096,
         'CVAR_SERVERINFO_NOUPDATE':8192}
FIELDS = ['name','module','scope','default','default_expression','flags',
          'flag_bits','conditions','kind','source','line']
HISTORY_FIELDS=['version','date','commit','action','name','before','after','subject']
SDK_BASE='82d3fca'
PRE_TCE='f8c5d52'
STAGES=('82d3fca','f8c5d52','fcf641e','37fd8bc','2734e91','9f86d22',
        '7707e8c','f7e1622','4770335','85705fa','fc5ae89')

def git(*args: str, check: bool=True) -> str:
    run = subprocess.run(['git',*args],cwd=ROOT,capture_output=True,check=check)
    return run.stdout.decode('utf-8','replace')

def comments(text: str) -> str:
    """Mask comments while retaining byte positions, strings and line numbers."""
    pattern = re.compile(STRING+r"|'(?:\\.|[^'\\])*'|//[^\n]*|/\*.*?\*/", re.S)
    return pattern.sub(lambda m: ''.join('\n' if c=='\n' else ' ' for c in m[0])
                       if m[0].startswith(('/',)) else m[0],text)

def balanced(text: str, at: int, opening='{', closing='}') -> int:
    depth=0; quote=''; escaped=False
    for i in range(at,len(text)):
        c=text[i]
        if quote:
            if escaped: escaped=False
            elif c=='\\': escaped=True
            elif c==quote: quote=''
        elif c in ('"',"'"): quote=c
        elif c==opening: depth+=1
        elif c==closing:
            depth-=1
            if not depth:return i
    raise ValueError('Unbalanced C initializer/call')

def split(text: str) -> list[str]:
    out=[]; begin=0; depth=0; quote=''; escaped=False
    for i,c in enumerate(text):
        if quote:
            if escaped: escaped=False
            elif c=='\\':escaped=True
            elif c==quote:quote=''
        elif c in ('"',"'"):quote=c
        elif c in '({[':depth+=1
        elif c in ')}]':depth-=1
        elif c==',' and not depth:out.append(text[begin:i].strip());begin=i+1
    if text[begin:].strip():out.append(text[begin:].strip())
    return out

def literal(expression: str) -> str | None:
    parts=list(re.finditer(STRING,expression))
    if not parts or re.sub(STRING,'',expression).strip():return None
    try:return ''.join(ast.literal_eval(m[0]) for m in parts)
    except (SyntaxError,ValueError):return None

def flag_bits(expression: str) -> int | None:
    try:
        node=ast.parse(expression,mode='eval').body
        def evaluate(n):
            if isinstance(n,ast.Constant) and isinstance(n.value,int):return n.value
            if isinstance(n,ast.Name):return FLAGS[n.id]
            if isinstance(n,ast.BinOp) and isinstance(n.op,ast.BitOr):return evaluate(n.left)|evaluate(n.right)
            if isinstance(n,ast.BinOp) and isinstance(n.op,ast.BitAnd):return evaluate(n.left)&evaluate(n.right)
            if isinstance(n,ast.UnaryOp) and isinstance(n.op,ast.Invert):return ~evaluate(n.operand)
            raise ValueError(expression)
        return evaluate(node)
    except (SyntaxError,KeyError,ValueError):return None

def flag_names(bits: int | None, expression: str) -> str:
    if bits is None:return expression
    names=[name.removeprefix('CVAR_') for name,bit in FLAGS.items() if bits&bit]
    unknown=bits & ~sum(FLAGS.values())
    if unknown:names.append(hex(unknown))
    return '|'.join(names) or '0'

def line_guards(text: str) -> list[str]:
    result=[]; stack=[]
    for line in text.splitlines():
        m=re.match(r'^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b\s*(.*)',line)
        if m:
            op,value=m.groups();value=value.strip()
            if op=='ifdef':stack.append([f'defined({value})',f'defined({value})'])
            elif op=='ifndef':stack.append([f'!defined({value})',f'!defined({value})'])
            elif op=='if':stack.append([value,value])
            elif op=='elif' and stack:
                before=stack[-1][1];stack[-1]=[f'!({before}) && ({value})',f'({before}) || ({value})']
            elif op=='else' and stack:stack[-1][0]=f'!({stack[-1][1]})'
            elif op=='endif' and stack:stack.pop()
        result.append(' && '.join(f'({state[0]})' for state in stack))
    return result

def module(path: str) -> str:
    if path.startswith('src/game/'):return 'QGame'
    if path.startswith('src/cgame/'):return 'CGame'
    if path.startswith('src/botai/'):return 'SDK BotAI'
    if path.endswith(('ui_shared.c','tce_modern_ui.c','tce_modern_theme.inc')):return 'UI+CGame'
    return 'UI'

def feature_required(condition: str, feature: str) -> bool:
    """Prove that a Boolean guard cannot be true with this feature disabled.

    Other macros remain unknown. Unsupported arithmetic stays unclassified,
    rather than incorrectly labelling a public branch as editor-only.
    """
    if feature not in condition:return False
    expr=re.sub(r'\bdefined\s*\(\s*(\w+)\s*\)',r'\1',condition)
    tokens=re.findall(r'&&|\|\||!|\(|\)|[A-Za-z_]\w*|\d+|\S',expr)
    at=0
    def atom():
        nonlocal at
        if at>=len(tokens):raise ValueError('missing term')
        token=tokens[at];at+=1
        if token=='!':return {not value for value in atom()}
        if token=='(':
            value=disjunction()
            if at>=len(tokens) or tokens[at]!=')':raise ValueError('missing parenthesis')
            at+=1;return value
        if token==feature:return {False}
        if token.isdigit():return {bool(int(token))}
        if re.fullmatch(r'[A-Za-z_]\w*',token):return {False,True}
        raise ValueError('unsupported expression')
    def conjunction():
        nonlocal at
        values=atom()
        while at<len(tokens) and tokens[at]=='&&':
            at+=1;right=atom();values={left and value for left in values for value in right}
        return values
    def disjunction():
        nonlocal at
        values=conjunction()
        while at<len(tokens) and tokens[at]=='||':
            at+=1;right=conjunction();values={left or value for left in values for value in right}
        return values
    try:
        possible=disjunction()
        return at==len(tokens) and True not in possible
    except ValueError:return False

def scan_text(path: str, text: str) -> tuple[list[dict],list[dict]]:
    source=comments(text); guards=line_guards(source); rows=[]; unresolved=[]
    arrays={}
    for m in re.finditer(r'\bconst\s+char\s*\*\s*(\w+)\s*\[\s*\]\s*=\s*\{',source):
        end=balanced(source,m.end()-1)
        values=[literal(p) for p in split(source[m.end():end])]
        if all(v is not None for v in values):arrays[m[1]]=values
    def add(name,value,flags,at,kind):
        line=source.count('\n',0,at)+1;condition=guards[line-1] if line<=len(guards) else ''
        scope='Editor' if feature_required(condition,'FEATURE_NAVSTUDIO') else 'Game/Editor'
        if 'tce_navgen' in path:scope='Editor'
        if '/botai/' in path:scope='Legacy SDK AI'
        if '/tce_lua' in path:condition='FEATURE_LUA'+(' && '+condition if condition else '')
        bits=flag_bits(flags)
        rows.append(dict(name=name,module=module(path),scope=scope,
                         default=literal(value) if literal(value) is not None else '<'+value+'>',
                         default_expression=value,flags=flag_names(bits,flags),flag_bits='' if bits is None else bits,
                         conditions=condition,kind=kind,source=path,line=line))
    table_count=0
    for match in TABLE.finditer(source):
        end=balanced(source,match.end()-1);at=match.end()
        while at<end:
            if source[at]=='{':
                row_end=balanced(source,at);values=split(source[at+1:row_end])
                if len(values)<4 or literal(values[1]) is None:raise ValueError(f'Unknown cvar table row in {path}:{source.count(chr(10),0,at)+1}')
                add(literal(values[1]),values[2],values[3],at,'table');table_count+=1;at=row_end+1
            else:at+=1
    for match in CALL.finditer(source):
        end=balanced(source,match.end()-1,'(',')');args=split(source[match.end():end])
        if len(args)!=4:raise ValueError(f'Unexpected registration arity: {path}')
        prefix=source[source.rfind('\n',0,match.start())+1:match.start()]
        if re.search(r'\bvoid\s*$',prefix):continue # prototype/import wrapper
        name=literal(args[1])
        if name is not None:add(name,args[2],args[3],match.start(),'direct');continue
        if args[1] in ('cv->cvarName','cv->cvar_name'):
            if not table_count:raise ValueError(f'Uncovered table dispatcher: {path}')
            continue
        array=re.fullmatch(r'(\w+)\[(\w+)\]',args[1])
        if array and array[1] in arrays:
            defaults=re.fullmatch(r'(\w+)\['+array[2]+r'\]',args[2])
            if defaults and defaults[1] in arrays and len(arrays[defaults[1]])==len(arrays[array[1]]):
                for n,v in zip(arrays[array[1]],arrays[defaults[1]]):add(n,json.dumps(v,ensure_ascii=False),args[3],match.start(),'paired-array')
                continue
        formatted=re.fullmatch(r'va\(\s*('+STRING+r')\s*,\s*(\w+)\[(\w+)\]\s*\)',args[1])
        defaults=re.fullmatch(r'(\w+)\[(\w+)\]',args[2])
        if formatted and defaults and formatted[2] in arrays and defaults[1] in arrays and len(arrays[formatted[2]])==len(arrays[defaults[1]]):
            for channel,value in zip(arrays[formatted[2]],arrays[defaults[1]]):
                add(literal(formatted[1]).replace('%s',channel),json.dumps(value,ensure_ascii=False),args[3],match.start(),'formatted-array')
            continue
        unresolved.append(dict(source=path,line=source.count('\n',0,match.start())+1,name=args[1],default=args[2],flags=args[3]))
    return rows,unresolved

def source_paths(ref: str) -> list[str]:
    if ref == 'WORKTREE':
        paths=git('ls-files','--cached','--others','--exclude-standard','--','src').splitlines()
        candidates=sorted(set(p for p in paths if p.endswith(SOURCE_SUFFIXES)
                      and not p.startswith(('src/tests/','src/reconstruction/','src/vendor/'))))
        return [p for p in candidates if (ROOT/p).is_file()
                and re.search(r'trap_Cvar_Register|cvarTable_t',(ROOT/p).read_text(encoding='utf-8',errors='replace'))]
    paths=git('grep','-l','-E','trap_Cvar_Register|cvarTable_t',ref,'--','src',check=False).splitlines()
    return sorted(p.split(':',1)[1] for p in paths if ':' in p and p.endswith(SOURCE_SUFFIXES)
                  and not p.split(':',1)[1].startswith(('src/tests/','src/reconstruction/','src/vendor/')))

def snapshot(ref: str) -> tuple[list[dict],list[dict]]:
    rows=[];unresolved=[]
    for path in source_paths(ref):
        text=(ROOT/path).read_text(encoding='utf-8',errors='replace') if ref=='WORKTREE' else git('show',f'{ref}:{path}')
        found,other=scan_text(path,text);rows+=found;unresolved+=other
    return sorted(rows,key=lambda r:(r['name'].casefold(),r['module'],r['source'],r['line'])),unresolved

def canonical_default(value: str) -> str:
    try:
        number=Decimal(value)
        if number.is_finite():return str(number.normalize())
    except InvalidOperation:pass
    return value

def signatures(rows: list[dict]) -> dict[str,tuple]:
    result=defaultdict(set)
    for row in rows:
        conditions=re.sub(r'\s+','',row['conditions'])
        result[row['name'].casefold()].add((row['module'],row['scope'],canonical_default(row['default']),
                                           str(row['flag_bits']),conditions))
    return {name:tuple(sorted(values)) for name,values in result.items()}

def summary(signature: tuple) -> str:
    return '; '.join(f'{module}/{scope}: {value!r}, flags={bits or "symbolic"}'+
                     (f', if {condition}' if condition else '')
                     for module,scope,value,bits,condition in signature)

def version_at(ref: str) -> str:
    text=git('show',f'{ref}:src/game/tce_version.h',check=False)
    match=re.search(r'^\s*#define\s+TCE_VERSION\s+"([^"]+)"',text,re.M)
    return match[1] if match else 'pre-1.0.0'

def history(ref='HEAD') -> tuple[list[dict],dict[str,str]]:
    """Incremental replay of reachable source changes, retaining transient knobs."""
    candidates=set()
    for stage in STAGES+(ref,):
        if subprocess.run(['git','merge-base','--is-ancestor',stage,ref],cwd=ROOT,capture_output=True).returncode:
            continue
        candidates.update(source_paths(stage))
    # Include the version marker even when no registration body changes.
    paths=sorted(candidates|{'src/game/tce_version.h'})
    state={};events=[];introduced={}
    for path in source_paths(SDK_BASE):
        rows,unresolved=scan_text(path,git('show',f'{SDK_BASE}:{path}'))
        if unresolved:raise ValueError(f'Uncovered baseline calls: {unresolved}')
        state[path]=rows
    before=signatures([r for rows in state.values() for r in rows])
    for name in before:introduced[name]='ET SDK baseline'
    commits=git('log','--reverse','--format=%H\t%cs\t%s',SDK_BASE+'..'+ref,'--',*paths).splitlines()
    for index,record in enumerate(commits,1):
        commit,date,subject=record.split('\t',2)
        changed=git('diff-tree','--no-commit-id','--name-only','-r',commit,'--',*paths).splitlines()
        for path in changed:
            if path not in candidates:continue
            text=git('show',f'{commit}:{path}',check=False)
            rows,unresolved=scan_text(path,text)
            if unresolved:raise ValueError(f'Uncovered historical call {commit[:7]}: {unresolved}')
            state[path]=rows
        current_rows=[r for rows in state.values() for r in rows]
        after=signatures(current_rows)
        for name in sorted(before.keys()|after.keys()):
            if before.get(name)==after.get(name):continue
            action='added' if name not in before else 'removed' if name not in after else 'changed'
            display=next((r['name'] for r in current_rows if r['name'].casefold()==name),name)
            version=version_at(commit)
            if action=='added' and name not in introduced:introduced[name]=version+' ('+commit[:7]+')'
            events.append(dict(version=version,date=date,commit=commit[:7],action=action,name=display,
                               before=summary(before.get(name,())),after=summary(after.get(name,())),subject=subject))
        before=after
        if index%30==0:print(f'History: {index}/{len(commits)} source commits replayed',flush=True)
    return events,introduced

def annotated(rows: list[dict]) -> list[dict]:
    sdk,_=snapshot(SDK_BASE);legacy,_=snapshot(PRE_TCE)
    sdk_names={r['name'].casefold() for r in sdk};legacy_names={r['name'].casefold() for r in legacy}
    for row in rows:
        name=row['name'].casefold()
        row['origin']='ET SDK inherited' if name in sdk_names else 'pre-versioned TC:E registration' if name in legacy_names else 'TCE2 added registration'
        row['engine_mirror']='yes' if (name.startswith(('r_','s_','cl_','com_','net_','sv_','m_','in_','fs_'))
                             and name not in ('sv_official','sv_gametype')) or name in ('mapname','version','protocol','developer','sensitivity') else 'no'
    return rows

def tsv(rows: list[dict], fields: list[str]) -> str:
    output=io.StringIO(newline='');writer=csv.DictWriter(output,fields,delimiter='\t',lineterminator='\n',extrasaction='ignore')
    writer.writeheader();writer.writerows(rows);return output.getvalue()

def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ref',default='WORKTREE',help='Source revision or WORKTREE (default)')
    parser.add_argument('--check',action='store_true',help='Fail if inventory, description coverage, or requested history is stale')
    parser.add_argument('--out',type=Path,default=ROOT/'Docs/cvars-registration.tsv')
    parser.add_argument('--history',type=Path,help='Also replay reachable registration changes into this TSV')
    args=parser.parse_args();rows,unresolved=snapshot(args.ref)
    if unresolved:
        for row in unresolved:print('UNRESOLVED',row)
        return 1
    rows=annotated(rows)
    def emit(path,content):
        if args.check:
            if not path.exists() or path.read_text(encoding='utf-8')!=content:
                print(f'STALE: {path.relative_to(ROOT) if path.is_relative_to(ROOT) else path}')
                return False
            return True
        path.parent.mkdir(parents=True,exist_ok=True)
        path.write_text(content,encoding='utf-8',newline='\n')
        return True
    success=emit(args.out,tsv(rows,FIELDS+['origin','engine_mirror']))
    names={r['name'].casefold() for r in rows}
    print(f'{len(rows)} registration rows; {len(names-{""})} named cvars; {len(names)} keys including unnamed legacy key; 0 unresolved registrations')
    if args.check:
        with (ROOT/'Docs/cvars-descriptions.tsv').open(encoding='utf-8',newline='') as stream:
            descriptions=list(csv.DictReader(stream,delimiter='\t'))
        with (ROOT/'Docs/cvars-history.tsv').open(encoding='utf-8',newline='') as stream:
            events=list(csv.DictReader(stream,delimiter='\t'))
        described=[d['name'].casefold() for d in descriptions]
        required={r['name'].casefold() for r in rows if r['origin']!='ET SDK inherited'}
        required|={e['name'].casefold() for e in events if e['action']=='changed'}&names
        missing=required-set(described)
        if missing or len(described)!=len(set(described)):
            print(f'Description coverage failed: missing={sorted(missing)}, duplicate names={len(described)!=len(set(described))}')
            success=False
        else:print(f'Description coverage: {len(described)} names; all current added/changed registrations covered')
    if args.history:
        events,_=history('HEAD' if args.ref=='WORKTREE' else args.ref)
        success=emit(args.history,tsv(events,HISTORY_FIELDS)) and success
        print(f'{len(events)} reachable registration history events')
    return 0 if success else 1
if __name__=='__main__':raise SystemExit(main())
