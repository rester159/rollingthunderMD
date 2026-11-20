"""Diagnostic soak: walk/fire through each area with health refilled, so body
contact, guard recoil and hurt reactions occur repeatedly. Machine.run fails on
any rtAppFault. Reports contact hurts observed per area."""
import sys,struct,argparse
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent))
from test_soft_reset import Machine
from stage_entry_probe import enter_campaign_stage
p=argparse.ArgumentParser()
for n in ('rom','symbols','core'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--frames',type=int,default=2000)
p.add_argument('--stages',default='0,1,2,3,4,5,6,7,8,9')
p.add_argument('--mode',default='campaign_1p')
a=p.parse_args()
for stage in map(int,a.stages.split(',')):
    m=Machine(a);m.start(a.mode)
    if stage:enter_campaign_stage(m,a.rom,a.symbols,stage)
    hurts=recoils=0;last=0
    for f in range(a.frames):
        m.patch('scene',b'\x40',111);m.patch('scene',b'\0',107)
        m.buttons={(0,7)} if (f//240)%5!=4 else {(0,6)}
        if (f//60)%4==2:m.buttons.add((0,5))
        if f%90<2:m.buttons.add((0,0))
        if f%150<4:m.buttons.add((0,4))
        m.run(1)
        c=m.number('scene',1,109)
        if c and not last:hurts+=1
        last=c
        for i in range(39):
            e=m.read('scene',16,5196+i*36)
            if e[8]!=0xff and (e[9]&0xfc)==0x78 and e[11]==0:recoils+=1
    print('stage',stage,'contact reactions',hurts,'recoil frames',recoils,'fault',m.number('rtAppFault',2),flush=True)
    m.core.retro_unload_game();m.core.retro_deinit()
