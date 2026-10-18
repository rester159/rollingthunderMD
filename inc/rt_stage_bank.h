#ifndef RT_NATIVE_STAGE_BANK_H
#define RT_NATIVE_STAGE_BANK_H
#include "rt_world.h"
#include "rt_rom_layout.h"
#ifdef RT_LINEAR_ROM
#define RT_STAGE_BANK_BASE 0UL
#else
#define RT_STAGE_BANK_BASE 0x100000UL
#endif
#define RT_STAGE_BANK_BYTES 0x300000UL
/* Six consecutive512KiB pages replace only CPU windows2..7. The builder must
 * prove code, interrupts, stack initializer and persistent data stay below1MiB,
 * and the complete referenced bank is physically present in the cartridge.
 * Header ROM-end must record the physical page-aligned image end, not the
 * CPU-visible4MiB window. Out-of-image banks are rejected before mapping.
 * Stage pointers/caches become invalid across this call. Before switching,
 * terminate stage callbacks and release their references, including audio.
 * Compact builds place headers at generated offsets within window 2 and
 * reserve window 7 for temporary shared-pose DMA. Window 1 may be mirrored
 * into window 2 for compact UI assets; fixed code in windows 0/1 is unchanged.
 * The optional shared layout reserves windows 5/6 for immutable metadata;
 * its linker requires stage-local data to end before CPU address 0x280000. */
#ifdef __m68k__
u8 RT_stageBankSelect(u8 firstPage);
#endif
#endif
