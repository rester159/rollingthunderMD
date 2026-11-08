#!/usr/bin/env python3
"""Seed exhausted lives/credits, then exercise the production terminal flow."""
import argparse,hashlib,json
from pathlib import Path
from test_soft_reset import Machine
p=argparse.ArgumentParser(description=__doc__)
for n in ('rom','symbols','core','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--case',choices=['campaign_1p','campaign_2p','survival_1p','survival_2p','arcade'],default='campaign_1p')
a=p.parse_args();m=Machine(a);m.start(a.case)
# Target-compiler verified offsets: players.combat=104, home=124; combat=10B.
m.patch('scene',b'\0\0',124)
if a.case=='arcade':
 m.patch('score',(654321).to_bytes(4,'big'),4)
 m.run(3) # Let the existing score-award policy account for seeded points.
 m.patch('arcade',b'\0')
for player in range(2 if a.case.endswith('2p') else 1):
 m.patch('scene',b'\0',104+10*player+7) # health
 m.patch('scene',b'\1',104+10*player+9) # removed
m.run(4500 if a.case=='arcade' else 600)
assert m.number('rtAppState',2)!=1 or (a.case=='arcade' and m.number('rtArcadeAttract')), 'Exhausted session remained in gameplay'
if a.case=='arcade':assert m.number('rtHighStarted',2)==1, 'Did not visit game-over/high-score flow'
if a.case!='arcade':
 assert m.number('page')==6, ('expected game-over menu',m.number('page'))
 assert m.number('rtTitleMenuFault',2)==0,'Game-over title raster failed'
 m.press(3);m.run(300)
 assert m.number('rtTitleMenuFault',2)==0
m.core.retro_reset();m.run(300);m.start('campaign_1p')
r=dict(rom_sha256=hashlib.sha256(a.rom.read_bytes()).hexdigest(),case=a.case,terminal_flow=True,restart_after_reset=True,limits='Seeds health/removed/credits; does not simulate damage animation or an entire playthrough.')
a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r))
m.core.retro_unload_game();m.core.retro_deinit()
