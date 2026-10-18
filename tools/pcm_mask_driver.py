"""Timed mask decoder with a full interrupt polling window and threaded states.

Only documented Z80 instructions. The immutable dispatch table doubles as a
threaded stack. IRQ exchange restores the overwritten table word before return.
The local playback window restores IX from that same word before resuming.
"""
def driver(source,linear=False):
 def pad(n):
  for jumps in range(4):
   for loads in range(5):
    for refresh in range(2):
     rest=n-jumps*10-loads*7-refresh*9
     if rest>=0 and rest%4==0:
      return '            JP $+3\n'*jumps+'            LD A,0\n'*loads+'            LD A,R\n'*refresh+'            NOP\n'*(rest//4)
  raise ValueError(n)
 def code(label,body,cycles,next_state=True):
  # Every normal continuation consumes 112 cycles; common output prefix 115.
  tail=('            POP IX\n' if next_state else '')+'            JP sample_loop\n'
  return label+'\n'+body+pad(112-cycles-(14 if next_state else 0)-10)+tail
 def ins(*lines):return ''.join('            '+line+'\n' for line in lines)
 prefix=source[:source.index('            LD IY,normal\n')]
 prefix=prefix.replace('signature DB $52,$54,$50,$43,5','signature DB $52,$54,$50,$43,10')
 prefix=prefix.replace('irq         LD H,$11\n            RETI', 'irq         LD H,$11\n'+ins('EX (SP),IX','INC SP','INC SP','JP (IX)'))
 prefix=prefix.replace('            DS $200-$,0','            DS $120-$,0\npairEntries DW '+','.join(['pair'+str(i) for i in range(16)]+['read_mask']+['delta'+str(i) for i in range(16)]+['escape_index','escape_ready'])+'\n            DS $200-$,0')
 prefix=prefix.replace('LD SP,$1FF0','LD SP,$11F0').replace('resume DW normal','resume DW 0').replace('physicalPage DB 51','physicalPage DB 0\nlastSegment DB 0\ncodecMode DB 0')
 prefix+=ins('LD A,(codecMode)','AND 1','JR Z,cursor_ready','DEC E')+'cursor_ready\n'
 prefix+=ins('LD A,2','LD (state),A','CALL notify_status','LD SP,initial_program','POP IX')
 prefix+='sample_loop\n'+ins('LD A,(HL)','LD ($4001),A','INC L','LD H,D','EI')+'            NOP\n'*14+ins('DI','BIT 0,H','JR NZ,irq_path','JP (IX)')
 prefix+='irq_path\n'+ins('LD H,$10','EXX','LD B,64','EXX')+pad(20)+ins('JP local_sample')
 prefix+='local_sample\n'+ins('LD A,(HL)','LD ($4001),A','INC L','EXX','DEC B','EXX','JP Z,window_done')+pad(171)+ins('JP local_sample')
 prefix+='window_done\n'+pad(145)+ins('DEC SP','DEC SP','POP IX','JP sample_loop')
 prefix+='            DS $500-$,0\n'
 result=prefix
 for case in range(16):
  def sample(code):
   return (ins('LD A,(BC)','INC BC','NOP') if code==3 else ins('LD A,'+str((128,127,129)[code]),'LD ($11A0),A'))+ins('LD (DE),A','INC E')
  result+='pair'+str(case)+'\n'+ins('LD A,E','SUB L','CP 252','JR NC,full'+str(case))
  result+=sample(case>>2)+sample(case&3)+pad(4)+ins('POP IX','JP sample_loop')
  result+='full'+str(case)+'\n'+pad(75)+ins('JP sample_loop')
 # Delta mode keeps DE on the last written sample, rather than the next
 # free byte. Reading the predictor is local to Z80 RAM. Each common sample
 # saves six cycles; padding equalizes all sixteen pair paths to 112 cycles.
 for case in range(16):
  def delta_sample(code):
   body=ins('LD A,(BC)','INC BC','NOP') if code==3 else ins('LD A,(DE)','ADD A,'+str((0,255,1)[code]))
   return body+ins('INC E','LD (DE),A')
  common=int((case>>2)!=3)+int((case&3)!=3)
  result+='delta'+str(case)+'\n'+ins('LD A,E','SUB L','CP 251','JR NC,delta_full'+str(case))
  result+=delta_sample(case>>2)+delta_sample(case&3)+pad(4+6*common)+ins('POP IX','JP sample_loop')
  result+='delta_full'+str(case)+'\n'+pad(75)+ins('JP sample_loop')
 read_body=ins('LD A,(BC)','INC BC','EXX','LD L,A','LD H,$1F','LD B,(HL)','DEC H','LD L,(HL)','LD H,B','LD SP,HL','EXX')
 result+='read_mask\n'+ins('EXX','INC E','JR Z,count_carry','EXX','INC SP')+read_body+ins('POP IX','JP sample_loop')
 result+='count_carry\n'+ins('INC D','JR Z,group_end','EXX','LD SP,unchecked_program')+pad(43)+ins('POP IX','JP sample_loop')
 result+='group_end\n'+ins('EXX','LD SP,segment_program')+pad(38)+ins('POP IX','JP sample_loop')
 result+=code('mask_unchecked',read_body,63)
 # Escape masks select the original six-byte program directly. The index
 # calculation fits one timed continuation; publication is a second one.
 result+=code('escape_index',ins('LD A,(BC)','INC BC','EXX','LD L,A','LD H,0','LD C,L','LD B,H','ADD HL,HL','ADD HL,BC','ADD HL,HL','LD A,H','ADD A,$12','LD H,A','EXX'),88)
 result+=code('escape_ready',ins('EXX','LD SP,HL','EXX'),14)
 result+=code('header_low',ins('LD A,(BC)','INC BC','CPL','EXX','LD E,A','EXX'),32)
 result+=code('header_high',ins('LD A,(BC)','INC BC','LD (lastSegment),A','AND $7F','CPL','EXX','LD D,A','EXX'),52)
 result+='check_last\n'+ins('LD A,(lastSegment)','BIT 7,A','JP NZ,stream_done')+pad(57)+ins('POP IX','JP sample_loop')
 result+='stream_done\n'+ins('LD A,(tailRemaining)','LD E,A','LD SP,$11F0')+pad(44)+ins('JP drain_loop')
 if linear:
  result+=code('bank_begin',ins('EXX','LD HL,(bankValue)','INC HL','LD (bankValue),HL','EXX'),46)
 else:
  result+='bank_begin\n'+ins('EXX','LD HL,(bankValue)','INC HL','LD (bankValue),HL','LD A,L','CP $20','JP Z,mapper_overflow','EXX')+pad(21)+ins('POP IX','JP sample_loop')
  result+='mapper_overflow\n'+ins('EXX','LD SP,mapper_program')+pad(11)+ins('POP IX','JP sample_loop')
 result+=code('bank_bit',ins('EXX','LD A,L','AND 1','LD ($6000),A','SRL H','RR L','EXX'),48)
 if linear:
  # The Z80's native 32 KiB bank follows physical ROM directly. Keep status
  # page bookkeeping without accessing cartridge mapper registers. Both
  # paths retain the same 227-cycle sample cadence.
  result+=code('bank_ready',ins('LD BC,$8000','LD A,(bankValue)','RRCA','RRCA','RRCA','RRCA','AND 7','LD (physicalPage),A'),59)
 else:result+=code('bank_ready',ins('LD BC,$8000'),10)
 result+=code('mapper_select',ins('EXX','LD HL,$142','EXX'),18)
 result+=code('mapper_write',ins('EXX','LD A,(physicalPage)','INC A','LD (physicalPage),A','LD ($B0F3),A','EXX'),54)
 result+=code('mapper_restore',ins('EXX','LD HL,$10','LD (bankValue),HL','EXX','LD SP,bank_bits_program'),44)
 result+='initial_program DW header_low,header_high,read_mask\nunchecked_program DW mask_unchecked\n'
 result+='segment_program DW check_last,bank_begin\nbank_bits_program DW '+','.join(['bank_bit']*9+['bank_ready','header_low','header_high','read_mask'])+'\n'
 result+='mapper_program DW '+','.join(['mapper_select']+['bank_bit']*9+['mapper_write','mapper_restore'])+'\n'
 result+=source[source.index('\ndrain_loop\n')+1:source.index('\nmapper_overflow\n            EXX')+1]
 # Normal finite drain and notification use the fixed stack, never the tables.
 result+='            DS $1000-$,0\n            END\n'
 return result
