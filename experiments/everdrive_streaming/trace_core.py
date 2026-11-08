#!/usr/bin/env python3
"""Build an isolated GX core with a host-only DMA entry recorder.

No guest instruction, timing, ROM or save is changed. Input must be a local
Genesis Plus GX source tree. The resulting core is diagnostic-only.
"""
import argparse
from pathlib import Path
import shutil
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--source', required=True, type=Path)
p.add_argument('--output', required=True, type=Path)
a = p.parse_args()
assert a.source.resolve() != a.output.resolve()
shutil.copytree(a.source, a.output, dirs_exist_ok=True,
                ignore=shutil.ignore_patterns('.git', '*.o', '*.dylib', '*.so', 'builds'))
source = a.output/'core/m68k/m68kcpu.c'
text = source.read_text()
declarations = '''
/* Streaming feasibility: host-only reads of guest stack RAM at DMA entry. */
extern unsigned char work_ram[0x10000];
unsigned int rt_dma_watch_pc, rt_dma_used, rt_dma_argument_offset;
/* Diagnostic-only stop before a return address; zero leaves gameplay unchanged. */
unsigned int rt_diagnostic_stop_pc;
unsigned int rt_dma_events[16384 * 3];
static unsigned int rt_dma_word(unsigned int address)
{
  unsigned int at = address & 65535;
#ifdef LSB_FIRST
  return (work_ram[at ^ 1] << 8) | work_ram[((at + 1) & 65535) ^ 1];
#else
  return (work_ram[at] << 8) | work_ram[(at + 1) & 65535];
#endif
}
'''
hook = '''
    if (rt_diagnostic_stop_pc && (REG_PC & 0xffffff) == rt_diagnostic_stop_pc)
      break;
    if (rt_dma_watch_pc && (REG_PC & 0xffffff) == rt_dma_watch_pc)
    {
      if (rt_dma_used < 16384)
      {
        rt_dma_events[rt_dma_used*3] = (rt_dma_word(REG_SP+(rt_dma_argument_offset ? rt_dma_argument_offset : 8)) << 16) | rt_dma_word(REG_SP+(rt_dma_argument_offset ? rt_dma_argument_offset : 8)+2);
        rt_dma_events[rt_dma_used*3+1] = rt_dma_word(REG_SP+18) * 2;
        rt_dma_events[rt_dma_used*3+2] = m68k.cycles;
      }
      rt_dma_used++;
    }
'''
needle = 'void m68k_run(unsigned int cycles)'
assert text.count(needle) == 1
text = text.replace(needle, declarations + '\n' + needle)
needle = '    /* Decode next instruction */'
assert text.count(needle) == 1
text = text.replace(needle, hook + '\n' + needle)
source.write_text(text)
source=a.output/'core/memz80.c'
text=source.read_text()
needle='void z80_memory_w(unsigned int address, unsigned char data)'
text=text.replace(needle,'''/* Host-only DAC writes, including their original master-cycle timestamps. */
unsigned int rt_dac_enabled,rt_dac_used,rt_dac_address;
unsigned int rt_dac_events[4096*2];
'''+needle)
needle='      fm_write(Z80.cycles, address & 3, data);'
assert text.count(needle)==1
text=text.replace(needle,'''      if (rt_dac_enabled)
      {
        if (!(address & 1)) rt_dac_address=data|((address & 2)<<7);
        else if (rt_dac_address==0x2a && zram[0x110]==0x52 && zram[0x111]==0x54 && zram[0x101]==2 && zram[0x100]==1)
        {
          if (rt_dac_used<4096)
          {
            rt_dac_events[rt_dac_used*2]=data;
            rt_dac_events[rt_dac_used*2+1]=Z80.cycles;
          }
          rt_dac_used++;
        }
      }
'''+needle)
source.write_text(text)
with (a.output/'streaming-build.log').open('w') as log:
    subprocess.run(['make', '-f', 'Makefile.libretro', '-j4', 'platform=osx',
                    'ARCHFLAGS=-arch arm64', 'CC=clang', 'CXX=clang++'],
                   cwd=a.output, stdout=log, stderr=subprocess.STDOUT, check=True)
print(a.output/'genesis_plus_gx_libretro.dylib')
