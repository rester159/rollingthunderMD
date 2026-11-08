#include <genesis.h>
#include "rt_save.h"
#include "rt_rom_layout.h"

extern u8 _start, _bend;
extern VoidCallback *intCB;
extern void __real__reset_entry(void);
static volatile u32 resetCookie[3];
static void defaultInterrupt(void) {}

void RT_resetMarkReady(void)
{
    resetCookie[0]=0;
    resetCookie[1]=~0x52544d44UL;
    resetCookie[2]=(u32)&_stext ^ *(const vu16 *)0x18e;
    resetCookie[0]=0x52544d44UL; /* publish only after save initialization */
}

/* Both cold entry and reset arrive after TMSS, with interrupts masked, Z80
 * halted and a fresh stack. Flash-cart menus may leave arbitrary RAM and I/O.
 * Retain only this build's committed save mirror, never an old callback. */
__attribute__((externally_visible, noinline, noreturn))
void __wrap__reset_entry(void)
{
#if RT_CART_EXTENDED_SSF
    /* A reset may interrupt a save. Close the cartridge write gate first. */
    *(vu16*)0xa130f0=0x8000;
#endif
    RT_SaveReset saved;
    bool warm=resetCookie[0]==0x52544d44UL && resetCookie[1]==~0x52544d44UL &&
        resetCookie[2]==((u32)&_stext ^ *(const vu16 *)0x18e);
    resetCookie[0]=0;
    if(warm)RT_saveResetExport(&saved);
    /* On a genuine reset the VDP may still own a DMA source. On cold entry,
     * avoid touching it until after RAM initialization: early VDP reads can
     * lock real consoles. SDK internal_reset waits again after initialization. */
    if(warm)while(GET_VDP_STATUS(VDP_DMABUSY_FLAG));
    /* The prior PCM owner may have replaced window 1. Restore fixed ROM data
     * before copying the .data image; this function executes in window 0. */
#ifndef RT_LINEAR_ROM
    *(vu8 *)0xa130f3=1;
#endif
    volatile u8 *dst=&_start;
    while(dst<&_bend)*dst++=0;
    const u8 *src=(const u8 *)&_stext;
    dst=(vu8 *)RAM;
    /* GCC 16 can combine *dst++=*src++ into one instruction that evaluates
     * the destination after incrementing the shared source address register.
     * Keep the accesses separate, as in our guarded Z80 uploader. */
    for(u32 n=(u32)&_sdata;n;n--)
        __asm__ volatile("move.b (%0)+,%%d0\n\tmove.b %%d0,(%1)+"
                         : "+a"(src), "+a"(dst) : : "d0", "memory");
    /* Unlike _start_entry, we keep the active stack intact. The SDK reset
     * below rebuilds heap, DMA, VDP, sound and callbacks after global reset. */
    intCB=defaultInterrupt;
    if(warm)RT_saveResetImport(&saved);
    /* The mirror has been imported. Discard this wrapper's temporary stack
     * before entering the non-returning SDK runtime; otherwise the larger
     * SRAM snapshot permanently consumes gameplay's reserved stack space. */
    __asm__ volatile("move.l #0xe1000000,%%sp\n\tjmp (%0)"
                     : : "a"(__real__reset_entry) : "memory");
    __builtin_unreachable();
}
