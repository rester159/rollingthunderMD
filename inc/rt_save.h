#ifndef RT_SAVE_H
#define RT_SAVE_H
#include <genesis.h>
/* Two CRC-checked slots; scratch/initial-entry controls are never persisted.
 * Call only with ROM DMA drained and outside an active entry IRQ. */
bool RT_saveLoad(u8 records[35],u32 *best,u8 *credits,u8 *psgMuted,u8 *controls);
bool RT_saveStore(const u8 records[35],u32 best,u8 credits,u8 psgMuted,u8 controls);
/* Warm reset carries only committed, CRC-checked slots across C global reset.
 * Export/import never access SRAM or perform a new save. */
typedef struct {u8 slots[2][64],available;} RT_SaveReset;
void RT_saveResetExport(RT_SaveReset *state);
void RT_saveResetImport(const RT_SaveReset *state);
extern volatile u8 rtSaveAvailable;
/* Publish this build's reset marker only after persistent state is loaded. */
void RT_resetMarkReady(void);
#endif
