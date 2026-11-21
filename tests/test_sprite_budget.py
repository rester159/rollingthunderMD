#!/usr/bin/env python3
"""Crowded scenes must not abort with UNFINISHED GRAPHICS (rtAppFault 33/34).

The HUD reserves 22 of the 80 SAT links. With many visible enemies plus shots,
RT_campaignSpritesQueue used to fail the whole frame once the next enemy or
shot did not fit, ending the game on the "UNFINISHED GRAPHICS / PREPARE FAILED"
screen. A seeded walker mostly holds right and also backs off, fires,
crouch-fires, tries doors and jumps between floors. Players are held
invincible (the only RAM write) so the enemy crowd builds up: since v0.93's
body-contact damage, unprotected input-only routes die before a crowd forms.
v0.92 also reached fault 34 input-only (Survival 1P seed 4, frame 2243). The
fixed build must stay in gameplay and records how many actors the SAT budget
left out of a frame.
"""
import argparse,hashlib,json,random,struct
from pathlib import Path
from test_soft_reset import Machine
p=argparse.ArgumentParser(description=__doc__)
for n in ('rom','symbols','core','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--case',choices=('survival_1p','survival_2p','campaign_1p'),default='survival_1p')
p.add_argument('--seed',type=int,default=4)
p.add_argument('--frames',type=int,default=6000)
a=p.parse_args();m=Machine(a);m.start(a.case)
players=2 if a.case.endswith('2p') else 1
RIGHT,LEFT,UP,DOWN,JUMP,FIRE=7,6,4,5,1,0
rng=random.Random(a.seed)
def x0():return struct.unpack('>i',m.read('scene',4,20))[0]
plan=[];last=None;still=0;end='frames'
for f in range(a.frames):
 if not plan:
  r=rng.random();x=x0();still=still+1 if x==last else 0;last=x
  if still>3:still=0;plan=[set(rng.choice([(DOWN,JUMP),(UP,JUMP),(JUMP,)]))|{RIGHT}]*6+[{RIGHT}]*30
  elif r<0.70:plan=[{RIGHT}]*rng.randint(10,60)
  elif r<0.78:plan=[{LEFT}]*rng.randint(10,90)
  elif r<0.85:plan=[{FIRE}]*3+[set()]*rng.randint(5,20)
  elif r<0.90:plan=[{UP}]*6+[set()]*rng.randint(10,120)
  elif r<0.95:plan=[{UP,JUMP,RIGHT}]*6+[{RIGHT}]*20
  else:plan=[{DOWN,FIRE}]*4+[{DOWN}]*rng.randint(5,30)
 held=plan.pop(0);m.buttons={(q,k) for q in range(players) for k in held}
 for q in range(players):m.patch('scene',b'\xff\xff',104+10*q)  # invincibleTicks
 if m.number('rtAppState',2)!=1 and f%20<3:m.buttons|={(q,3) for q in range(players)}  # continue
 m.core.retro_run()
 fault=m.number('rtAppFault',2)
 assert not fault,('Runtime fault',fault,'frame',f+1)
 if m.number('rtAppState',2)==0:end='game_over';break
if 'rtSpriteBudgetDrops' in m.symbols:
 assert m.number('rtSpriteBudgetDrops',4),'Route no longer overloads the SAT; choose another seed'
drops=m.number('rtSpriteBudgetDrops',4) if 'rtSpriteBudgetDrops' in m.symbols else None
r=dict(rom_sha256=hashlib.sha256(a.rom.read_bytes()).hexdigest(),case=a.case,seed=a.seed,
 frames=f+1,ended=end,budget_drops=drops,survival_retirements=m.number('rtAppSurvivalRetirements',4),
 limits='Controller input with players held invincible; one seeded route, not a full playthrough.')
a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r))
m.core.retro_unload_game();m.core.retro_deinit()
