#!/usr/bin/env python3
"""Controller-driven all-track sound menu regression, including rapid restarts."""
import argparse,hashlib,json
from pathlib import Path
from test_soft_reset import Machine
p=argparse.ArgumentParser(description=__doc__)
for n in ('rom','symbols','core','output'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();m=Machine(a)
for button in (5,3,5,5,3,5,5,3):m.press(button)
assert m.number('page')==8, 'Did not reach sound test'
rows=[]
for track in range(1,14):
 assert m.number('soundTrack')==track
 for repeat in range(3):
  m.press(3);m.run(120 if repeat<2 else 600)
  assert m.number('rtGameMusicTrack')==track
  assert m.number('page')==8
  assert m.number('rtTitleMenuFault',2)==0, ('title raster fault',track,repeat)
  before=m.number('rtTitleMenuFrames',4)
  recovered=m.number('rtTitleMenuRasterRecoveries',4) if 'rtTitleMenuRasterRecoveries' in m.symbols else 0
  m.run(60)
  assert m.number('rtTitleMenuFrames',4)>before, ('animation froze',track,repeat)
  if 'rtTitleMenuRasterRecoveries' in m.symbols:
   assert m.number('rtTitleMenuRasterRecoveries',4)==recovered, ('repeated steady-state raster misses',track,repeat)
 rows.append(track);m.press(7)
m.press(0);assert m.number('page')==5
m.core.retro_reset();m.run(300);m.start('campaign_1p')
r=dict(rom_sha256=hashlib.sha256(a.rom.read_bytes()).hexdigest(),tracks=rows,restarts_per_track=3,steady_menu_animation=True,no_steady_raster_misses=True,returned_to_settings=True,gameplay_after_reset=True)
a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r))
m.core.retro_unload_game();m.core.retro_deinit()
