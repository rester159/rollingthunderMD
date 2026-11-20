#!/usr/bin/env python3
"""Rebuild selected stage banks from source and repin their frozen hashes.

Use only after an intentional, reviewed art edit (for example
tools/fix_stage_windows.py or tools/fix_fan_colors.py). Prints previous and
new hashes for the changed stages. Usage: repin_stage_banks.py SGDK STAGE...
"""
import hashlib, json, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import build

sdk = Path(sys.argv[1]); wanted = {int(s) for s in sys.argv[2:]}
manifest = ROOT / 'assets/stages/manifest.json'; rows = json.loads(manifest.read_text()); changes = []
game = ROOT / '.build/game'
for row in rows:
    if row['stage'] not in wanted:
        continue
    old = row['sha256']
    try:
        build.build_stage(row, sdk, ROOT / '.build', game / 'inc', 8)
    except ValueError as exc:
        assert str(exc).startswith('Stage ' + str(row['stage']) + ' differs from frozen bank:'), str(exc)
    data = (ROOT / '.build/stages' / str(row['stage']) / 'bank.bin').read_bytes()
    row['sha256'] = hashlib.sha256(data).hexdigest(); row['bytes'] = len(data)
    changes.append(dict(stage=row['stage'], previous_sha256=old, sha256=row['sha256'], bytes=len(data)))
manifest.write_text(json.dumps(rows, indent=2) + '\n')
print(json.dumps(changes, indent=2))
