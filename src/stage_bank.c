#include "rt_stage_bank.h"
#include "rt_rom_layout.h"
#ifdef __m68k__
u8 RT_stageBankSelect(u8 firstPage)
{
    if(firstPage<1 || firstPage>58 || (VDP_getReg(1)&0x40) ||
       DMA_getQueueSize() || SPR_isInitialized())return 0;
    /* Packer records the physical image's inclusive end in the ROM header.
     * Reject absent pages rather than relying on emulator address wrapping. */
    u32 end=*(const volatile u32*)0x1a4;
    if(end>=0x2000000UL || (end&0x7ffffUL)!=0x7ffffUL)return 0;
#ifdef RT_PACKED_PATTERNS
    /* Compact stage metadata may occupy fewer than six physical pages. The
     * builder verifies every referenced byte; unused windows map safe page 0. */
    if((u32)firstPage*0x80000UL>end)return 0;
#else
    if(((u32)firstPage+6)*0x80000UL>end+1)return 0;
#endif
    /* Install stage/shared mappings atomically. Queued pixel DMA temporarily
     * uses window 7 and restores it before returning to the renderer. */
#ifndef RT_LINEAR_ROM
    SYS_disableInts();
    for(u16 region=2;region<8;region++){
        u16 page=firstPage+region-2;
        SYS_setBank(region,(u32)page*0x80000UL<=end?page:0);
    }
#ifdef RT_SHARED_METADATA_PAGE
    /* Immutable geometry/pose tables stay mapped during gameplay. Window 7
     * remains the transient pixel-DMA window and saves exclusively use 4. */
    SYS_setBank(5,RT_SHARED_METADATA_PAGE);
    SYS_setBank(6,RT_SHARED_METADATA_PAGE+1);
#endif
    SYS_enableInts();
#endif
    return 1;
}
#endif
