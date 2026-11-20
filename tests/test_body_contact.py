#!/usr/bin/env python3
"""Player/guard body contact through the production Campaign loop.

Arcade evidence (rev 3, MAME traces): CPU2 B298 calls 962A for every visible
ordinary actor. After the shared hitbox geometry, decision bit 0x40 (bodies
overlap on the same floor) damages the selected player by 0x20 and stores the
actor decisions in player record13 ($041D, written at 9696), which sends the
player into hurt state 0x78/0x79. While record13 is set, a touching actor
enters its own 0x78 recoil (96AF -> B429). In the arcade, a player walking
into the first Area 1 Masker is knocked back as soon as the bodies overlap.

Hold Right from the Area 1 start without firing. The first guard must hurt
the player within 20 updates of the bodies meeting, then recoil; v0.92 let
the player walk straight through it. Not a playthrough; no RAM is seeded.
"""
import argparse,hashlib,json,struct
from pathlib import Path
from test_soft_reset import Machine
p=argparse.ArgumentParser(description=__doc__)
for n in ('rom','symbols','core','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--case',choices=('walk',),default='walk')
a=p.parse_args();m=Machine(a);m.start('campaign_1p')
# Target-compiler offsets: scene.players.agents=20 (body.x at +0), combat=104
# (counterattack +5, health +7), enemies=5196 stride 36 (kind +8, action +9).
m.buttons={(0,7)}
hurt=recoil=None;contact=None
for frame in range(400):
    m.run(1)
    px,py=struct.unpack('>ii',m.read('scene',8,20))
    health=m.number('scene',1,111);counter=m.number('scene',1,109)
    for i in range(39):
        e=m.read('scene',36,5196+i*36)
        if e[8]==0xff:continue
        ex,ey=struct.unpack('>ii',e[:8])
        if abs(ex-px)<16*16 and abs(ey-py)<16*16 and contact is None:contact=frame
        if hurt is not None and (e[9]&0xfc)==0x78 and recoil is None:recoil=frame
    if hurt is None and (health<64 or counter):hurt=frame
    if hurt is not None and recoil is not None and frame>hurt+60:break
assert contact is not None,'No guard reached the player'
assert hurt is not None and hurt-contact<=20,('Player passed through the guard without contact damage',contact,hurt)
assert recoil is not None,'Touching guard did not recoil while the player reacted'
assert m.number('rtAppFault',2)==0
r=dict(rom_sha256=hashlib.sha256(a.rom.read_bytes()).hexdigest(),case=a.case,
       first_close_frame=contact,player_hurt_frame=hurt,guard_recoil_frame=recoil,
       health_after=m.number('scene',1,111),
       limits='Area 1 opening guard, 1P Campaign, production input and update path only.')
a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r))
m.core.retro_unload_game();m.core.retro_deinit()
