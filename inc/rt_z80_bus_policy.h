#ifndef RT_Z80_BUS_POLICY_H
#define RT_Z80_BUS_POLICY_H
#include <genesis.h>

/* Project-internal capability. Default/reset is FALSE. Only a driver whose
   complete execution stays off the main bus may opt in after installation.
   Every SGDK driver load revokes it, including another custom driver. This
   never exempts actual 68000 access to Z80 RAM from bus ownership. */
void RT_setZ80LocalBusOnly(bool enabled);
/* Set after installing a custom driver that reads cartridge ROM: DMA then
   always halts it. Every driver load clears it. */
void RT_setZ80RomDriver(bool enabled);
bool RT_z80NeedsMainBusHold(void);
/* Around a VDP DMA: returns a token for RT_z80DmaGuardEnd. */
u8 RT_z80DmaGuardBegin(void);
void RT_z80DmaGuardEnd(u8 token);
/* Running count of PROTECT handshakes that timed out and fell back to
   halting the ROM-reading driver for the whole transfer. Diagnostic only. */
u32 RT_z80DmaGuardFallbackCount(void);
/* Changes on every actual sound-driver installation, including custom loads.
   Compare ownership inside the existing driver-load critical section. */
u32 RT_z80DriverGeneration(void);
#endif
