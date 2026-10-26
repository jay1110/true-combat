"""Emit the complete manual release matrix from the checked-in target table."""
import json
import os
from pathlib import Path
from package import ROOT, version

rows = json.loads((ROOT / 'tools/release/targets.json').read_text())['targets']
variants = ('game', 'navstudio') if os.environ.get('INCLUDE_EDITOR', 'true').lower() == 'true' else ('game',)
matrix = {'include': [{'target': row['id'], 'runner': row['runner'], 'variant': variant}
                      for row in rows for variant in variants]}
output = {'matrix': json.dumps(matrix, separators=(',', ':')), 'version': version()}
if os.environ.get('GITHUB_OUTPUT'):
    with Path(os.environ['GITHUB_OUTPUT']).open('a', encoding='utf-8') as stream:
        for key, value in output.items():
            stream.write(key + '=' + value + '\n')
print(json.dumps(output, indent=2))
