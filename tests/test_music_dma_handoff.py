#!/usr/bin/env python3
"""Exercise the DMA-release race in the actual Z80 driver in Genesis Plus GX.

Pause at ACK clearing after PROTECT was read as zero; assert a new DMA request
before executing that instruction. A stale ACK must never let the driver leave
its RAM-only park while the new request remains active. ROM bytes are unchanged.
"""
import argparse,ctypes as C,json
from pathlib import Path
from test_soft_reset import Machine
class Z80(C.Structure):
    _fields_=[(n,C.c_uint32) for n in ('pc','sp','af','bc','de','hl','ix','iy','wz','af2','bc2','de2','hl2')]+[(n,C.c_uint8) for n in ('r','r2','iff1','iff2','halt','im','i','nmi_state','nmi_pending','irq_state','after_ei')]+[('cycles',C.c_uint32)]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--rom',type=Path,required=True);p.add_argument('--symbols',type=Path,required=True);p.add_argument('--core',type=Path,required=True);a=p.parse_args()
 m=Machine(a);m.start('campaign_1p');core=m.core
 z=Z80.in_dll(core,'Z80');ram=(C.c_uint8*8192).in_dll(core,'zram');data=bytes(ram)
 # The park exit: read PROTECT, test, loop if nonzero, clear ACK.
 pattern=bytes.fromhex('3a301fb720');positions=[i for i in range(0x1800) if data[i:i+5]==pattern and data[i+6:i+9]==bytes.fromhex('32311f')]
 assert len(positions)==1,positions
 entry=positions[0]+6;exit_pc=data.rfind(bytes.fromhex('d908c9'),0,entry)
 assert 0<entry-exit_pc<96
 core.z80_run.argtypes=[C.c_uint32]
 z.pc=entry;z.af=0x44;ram[0x1f30]=ram[0x1f31]=1
 escaped=False
 for _ in range(300):
  core.z80_run(z.cycles+1)
  if z.pc==exit_pc:escaped=True;break
 assert not escaped,'Z80 leaves its RAM-only DMA park with PROTECT still asserted (stale ACK race)'
 assert ram[0x1f31]==1,'New DMA was not acknowledged'
 ram[0x1f30]=0
 for _ in range(300):
  core.z80_run(z.cycles+1)
  if z.pc==exit_pc:break
 assert z.pc==exit_pc and ram[0x1f31]==0,'Driver did not release the completed DMA'
 print(json.dumps(dict(stale_ack_race_blocked=True,normal_release_pass=True,scope='Actual Z80 instruction execution with the race interleaving injected; not physical hardware audio certification.')))
 core.retro_unload_game();core.retro_deinit()
if __name__=='__main__':main()
