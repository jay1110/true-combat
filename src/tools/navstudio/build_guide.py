"""Build the static German/English in-app guide from the package README.

This deliberately supports only the Markdown constructs used by that README.
The generated guide executes no code and loads no external resources.
"""
from html import escape
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
STYLE = r'''
:root{color-scheme:dark;--bg:#14212a;--text:#e2edf3;--muted:#a8bcc8;--line:#3a4f5d;--accent:#81e3c5}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:14px/1.7 "Segoe UI",system-ui,sans-serif}
header{position:sticky;top:0;z-index:2;display:flex;align-items:center;gap:20px;padding:12px 22px;background:#20313c;border-bottom:1px solid var(--line)}header strong{font-size:16px}header a{font-size:12px;color:var(--accent)}
.layout{display:grid;grid-template-columns:205px minmax(0,1fr);gap:25px;max-width:1280px;margin:auto;padding:22px}
nav{position:sticky;top:65px;align-self:start;max-height:calc(100vh - 85px);overflow:auto;padding-right:9px;scrollbar-width:thin}
nav strong{display:block;color:var(--muted);font-size:11px;letter-spacing:1px;margin:12px 0 7px}nav a{display:block;color:#c3dbd9;text-decoration:none;font-size:11px;padding:4px 0}nav a:hover{text-decoration:underline;color:var(--accent)}
main{max-width:930px;min-width:0}h1{font-size:30px;margin:0 0 15px}h2{font-size:25px;border-top:1px solid var(--line);padding-top:26px;margin:30px 0 20px}h3{font-size:20px;color:#bde9dc;margin:30px 0 13px}h1,h2,h3{scroll-margin-top:70px}
p{margin:0 0 14px}ol,ul{margin:12px 0 20px;padding-left:24px}li{padding-left:3px;margin:8px 0}strong{color:#f3fafb}a{color:var(--accent)}
code{background:#223742;color:#bee7df;border:1px solid #3b5260;border-radius:3px;padding:1px 4px;font:12px/1.6 Consolas,monospace;overflow-wrap:anywhere}
pre{margin:17px 0;padding:14px;background:#0e171e;border:1px solid var(--line);border-radius:5px;overflow:auto}pre code{border:0;padding:0;background:none;white-space:pre-wrap}
.table-wrap{overflow:auto;margin:14px 0 22px}table{width:100%;border-collapse:collapse;font-size:12px}th,td{border:1px solid var(--line);padding:9px 11px;text-align:left;vertical-align:top}th{background:#243b46;color:#c5f0e0}td:first-child{width:35%}
footer{margin-top:35px;border-top:1px solid var(--line);padding:15px 0;color:var(--muted);font-size:11px}
@media(max-width:730px){.layout{grid-template-columns:1fr;padding:17px}nav{position:static;max-height:180px;border:1px solid var(--line);padding:10px;display:grid;grid-template-columns:1fr 1fr;gap:0 15px}nav strong{grid-column:1/-1}header{padding:10px 15px}body{font-size:13px}}
@media print{header,nav{display:none}.layout{display:block;padding:0}body{background:#fff;color:#111;font-size:10pt}strong,h1,h2,h3{color:#111}code,th{background:#eee;color:#111}pre{background:#fff}h2,h3{break-after:avoid}tr,pre{break-inside:avoid}}
'''


def inline(value):
    code = []
    def protect(match):
        code.append('<code>' + escape(match[1]) + '</code>')
        return '\x00' + str(len(code) - 1) + '\x00'
    value = re.sub(r'`([^`]+)`', protect, value)
    value = escape(value)
    value = re.sub(r'\*\*(.+?)\*\*', r'<strong>\1</strong>', value)
    return re.sub(r'\x00([0-9]+)\x00', lambda m: code[int(m[1])], value)


def slug(value):
    value = value.lower().replace('ä','a').replace('ö','o').replace('ü','u').replace('ß','ss')
    return re.sub(r'[^a-z0-9]+', '-', value).strip('-')


def render(markdown):
    lines = markdown.splitlines()
    result, contents, seen = [], [], set()
    scope, index = '', 0
    while index < len(lines):
        line = lines[index]
        if not line.strip(): index += 1; continue
        heading = re.match(r'^(#{1,4})\s+(.+)$', line)
        if heading:
            level, title = len(heading[1]), heading[2]
            if level == 2:
                scope = 'de' if title == 'Deutsch' else 'en' if title == 'English' else scope
            identity = (scope + '-' if scope else '') + slug(title)
            if identity in seen: raise ValueError('Duplicate guide heading: ' + title)
            seen.add(identity); contents.append((level, title, identity, scope))
            result.append(f'<h{level} id="{identity}">{inline(title)}</h{level}>');index += 1;continue
        if line.startswith('```'):
            block=[];index+=1
            while index<len(lines) and not lines[index].startswith('```'): block.append(lines[index]);index+=1
            if index==len(lines): raise ValueError('Unclosed code block')
            result.append('<pre><code>'+escape('\n'.join(block))+'</code></pre>');index+=1;continue
        if line.startswith('|'):
            rows=[]
            while index<len(lines) and lines[index].startswith('|'):
                rows.append([cell.strip() for cell in lines[index].strip('|').split('|')]);index+=1
            if len(rows)<2 or not all(re.fullmatch(r':?-+:?',cell) for cell in rows[1]): raise ValueError('Invalid Markdown table')
            def row(cells,tag): return '<tr>'+''.join('<'+tag+'>'+inline(c)+'</'+tag+'>' for c in cells)+'</tr>'
            result.append('<div class="table-wrap"><table><thead>'+row(rows[0],'th')+'</thead><tbody>'+''.join(row(r,'td') for r in rows[2:])+'</tbody></table></div>');continue
        ordered = re.match(r'^[0-9]+\.\s+',line)
        if ordered or line.startswith('- '):
            tag='ol' if ordered else 'ul';items=[]
            pattern=r'^[0-9]+\.\s+(.+)' if ordered else r'^-\s+(.+)'
            while index<len(lines):
                item=re.match(pattern,lines[index])
                if not item: break
                items.append('<li>'+inline(item[1])+'</li>');index+=1
            result.append('<'+tag+'>'+''.join(items)+'</'+tag+'>');continue
        paragraph=[]
        while index<len(lines) and lines[index].strip() and not re.match(r'^(?:#{1,4}\s|```|\||[0-9]+\.\s|-\s)',lines[index]):
            paragraph.append(lines[index]);index+=1
        result.append('<p>'+inline(' '.join(paragraph))+'</p>')
    links=[]
    for level,title,identity,_ in contents:
        if level==1: continue
        links.append('<strong>'+inline(title)+'</strong>' if level==2 else '<a href="#'+identity+'">'+escape(title)+'</a>')
    return ('<!doctype html>\n<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">'
            '<title>TCE2 NavStudio · Anleitung / User guide</title><style>'+STYLE+'</style></head><body>'
            '<header><strong>TCE2 NavStudio · Anleitung / User guide</strong><a href="#de-deutsch">Deutsch</a><a href="#en-english">English</a></header>'
            '<div class="layout"><nav aria-label="Inhalt / Contents">'+''.join(links)+'</nav><main>'+''.join(result)+
            '<footer>Lokale Hilfe aus README.md · Local help from README.md · <a href="#tce2-navstudio">↑</a></footer></main></div></body></html>\n')


def main():
    output = render((HERE/'README.md').read_text(encoding='utf8'))
    target = HERE/'guide.html'
    temporary = target.with_name('.guide-new.tmp')
    temporary.write_text(output,encoding='utf8',newline='\n');temporary.replace(target)
    print('Guide updated:',target)


if __name__ == '__main__':
    main()
