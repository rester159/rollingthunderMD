#!/usr/bin/env python3
"""Real menu DIP selection, persistence, demo mute and gameplay consumers.
Scene offsets are the m68000 ABI; symbol addresses come from this build.
"""
import argparse,hashlib,json
from pathlib import Path
from test_soft_reset import Machine
p=argparse.ArgumentParser(description=__doc__)
for n in ('rom','symbols','core','output'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();m=Machine(a)
addresses={r[2].split('.lto_priv')[0]:int(r[0],16) for l in a.symbols.read_text().splitlines() if len(r:=l.split())==3}
for b in (3,5,5,3):m.press(b)
assert m.number('page')==11
for i in range(6):m.press(7);m.press(5)
assert m.number('rtArcadeDips')==63
m.core.retro_reset();m.run(300);assert m.number('rtArcadeDips')==63
# Demo silence remains selected after reset; normal gameplay still has music.
m.press(3);m.press(3)
for _ in range(2200):
 m.run(1)
 if m.number('rtArcadeAttract')==2:break
assert m.number('rtArcadeAttract')==2
for _ in range(120):
 m.run(1);assert m.number('rtGameMusicTrack')==0
m.press(3);m.run(180)
assert m.number('rtAppState',2)==1 and not m.number('rtArcadeAttract')
assert m.number('arcade')==4
assert 110<m.number('scene',2,8056)<=120
assert m.number('scene',4,4)==addresses['rtEnemySpawnLevels']+10*12
assert m.number('rtGameMusicTrack')==6
m.buttons={(0,7),(0,0)}
for _ in range(900):
 m.run(1);assert m.number('scene',offset=111)==64,'Invulnerability lost health'
m.buttons.clear()
# These cabinet settings do not alter Home's default timer or authored set.
m.core.retro_reset();m.run(300);m.start('campaign_1p')
assert 140<m.number('scene',2,8056)<=150
assert m.number('scene',4,4)==addresses['rtEnemySpawnLevels']
# Restore default switches through the actual options item.
m.core.retro_reset();m.run(300)
for b in (3,5,3,5,5,3):m.press(b)
assert not m.number('rtArcadeDips')
m.core.retro_reset();m.run(300);assert not m.number('rtArcadeDips')
report=dict(rom_sha256=hashlib.sha256(a.rom.read_bytes()).hexdigest(),all_six_switches_persist=True,lives_and_timer=True,authored_enemy_set=True,invulnerability_900_refreshes=True,demo_muted_gameplay_music_retained=True,home_rules_independent=True,reset_defaults=True,physical_hardware_tested=False)
a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
m.core.retro_unload_game();m.core.retro_deinit()
