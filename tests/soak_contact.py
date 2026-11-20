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
    players=2 if a.mode.endswith('2p') else 1
    hurts=[0,0];recoils=0;last=[0,0]
    for f in range(a.frames):
        for q in range(players):m.patch('scene',b'\x40',111+10*q);m.patch('scene',b'\0',107+10*q)
        m.buttons=set()
        for q in range(players):
            g=(f+37*q)
            m.buttons.add((q,7) if (g//240)%5!=4 else (q,6))
            if (g//60)%4==2:m.buttons.add((q,5))
            if g%90<2:m.buttons.add((q,0))
            if g%150<4:m.buttons.add((q,4))
        m.run(1)
        for q in range(players):
            c=m.number('scene',1,109+10*q)
            if c and not last[q]:hurts[q]+=1
            last[q]=c
        for i in range(39):
            e=m.read('scene',16,5196+i*36)
            if e[8]!=0xff and (e[9]&0xfc)==0x78 and e[11]==0:recoils+=1
    print('stage',stage,'contact reactions',hurts,'recoil frames',recoils,'fault',m.number('rtAppFault',2),flush=True)
    m.core.retro_unload_game();m.core.retro_deinit()
