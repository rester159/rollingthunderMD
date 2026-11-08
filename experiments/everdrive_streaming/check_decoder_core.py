#!/usr/bin/env python3
"""Compare native decoder results/cycles with and without the host stop hook."""
import argparse,hashlib,json
from pathlib import Path
from types import SimpleNamespace
from probe import Diagnostic
from canonical_codec import normalize,encode

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--diagnostic',required=True,type=Path)
p.add_argument('--core',required=True,type=Path)
p.add_argument('--reference-core',required=True,type=Path)
p.add_argument('--format',choices=['canonical','fast'],default='canonical')
a=p.parse_args()
if a.format=='fast':
 from fast_codec import normalize,encode
fixtures=[bytes(range(256))*8,b'\xff'*2048,bytes(32),
 bytes.fromhex('efdfcdbcab9a897867564534231201f0')*100,
 bytes((i*73+i//17)%256 for i in range(1024))*2]
results=[]
for core in (a.reference_core,a.core):
 args=SimpleNamespace(core=core,rom=a.diagnostic/'diagnostic.bin',symbols=a.diagnostic/'diagnostic.symbols.txt')
 m=Diagnostic(args);m.setup(0x220);costs=[]
 for raw in fixtures:
  canonical,palette=normalize(raw);packed=palette+encode(canonical)
  costs.append(m.native(packed+bytes(len(packed)&1),raw,len(packed),colors=1+max(max(b>>4,b&15) for b in canonical)))
 m.core.retro_unload_game();m.core.retro_deinit()
 results.append(dict(core_sha256=hashlib.sha256(core.read_bytes()).hexdigest(),master_cycles=costs))
assert results[0]['master_cycles']==results[1]['master_cycles'],results
report=dict(format=a.format,fixtures=len(fixtures),exact_native_results_and_cycles=True,cores=results)
(a.diagnostic/'core-parity.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
