"""Diagnostic: player shots that pass through an enemy's body without a hit."""
import sys,struct,argparse,collections
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent))
from test_soft_reset import Machine
from stage_entry_probe import enter_campaign_stage
p=argparse.ArgumentParser()
for n in ('rom','symbols','core'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--frames',type=int,default=3000)
p.add_argument('--stage',type=int,default=0)
p.add_argument('--verbose',action='store_true')
a=p.parse_args();m=Machine(a);m.start('campaign_1p')
if a.stage:enter_campaign_stage(m,a.rom,a.symbols,a.stage)
reasons=collections.Counter();examples={}
for f in range(a.frames):
    m.patch('scene',b'\0\x10',104);m.patch('scene',b'\x40',111)
    m.buttons={(0,7)} if (f//200)%4!=3 else {(0,6)}
    if (f//50)%3==1:m.buttons.add((0,5))
    if f%24<2:m.buttons.add((0,0))
    m.run(1)
    if a.verbose and f%250==0:print(f,[m.read('scene',18,6768+i*18)[10] for i in range(49)],m.number('scene',1,111),m.read('scene',8,20).hex())
    shots=[]
    for i in range(49):
        s=m.read('scene',18,6768+i*18)
        x,y,vx=struct.unpack('>iih',s[:10])
        if s[10] in (1,3) and not s[14]&8:shots.append((x,y,vx,s[16]))
    for i in range(39):
        e=m.read('scene',36,5196+i*36)
        if e[8]==0xff or e[16]:continue
        ex,ey=struct.unpack('>ii',e[:8])
        for x,y,vx,layer in shots:
            if abs(x-ex)<10*16 and ey-56*16<y<ey-4*16:
                why=[]
                if e[14]!=(layer<<5)&255:why.append('layer')
                if e[13]&0x80:why.append('pose80')
                key=(e[8],e[9]&0xfc,tuple(why))
                reasons[key]+=1
                if key not in examples:examples[key]=(f,i,hex(e[9]),hex(e[13]),hex(e[14]),layer,(ex-x)//16,(ey-y)//16)
for k,v in reasons.most_common():print(k,v,examples[k])
