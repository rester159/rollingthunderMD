#ifndef RT_STAGE_TRANSITION_RASTER_H
#define RT_STAGE_TRANSITION_RASTER_H
#include "rt_stage_transition_background.h"

typedef struct
{
    volatile u16 index,fault,first,last,footer;
    volatile u32 interrupts;
} RT_StageTransitionRasterStats;

/* Exclusive HInt/CRAM owner for a validated RTPG scene. Call while display is
   blank, after background loading. Caches palettes/packets in RAM; the IRQ
   never reads/remaps ROM. Failed validation leaves the running owner intact.
   The caller must call Service from VInt, and Stop before another VDP owner
   takes over, including before blocking sound-driver reset/unload.
   No actor, timer, input, DMA queue or display-mode changes. */
bool RT_startStageTransitionRaster(const RT_StageTransitionBackground *background);
/* Native offline-compiled assets need no serialized RTPG wrapper. Same
   transactional validation, palette caching, and exclusive IRQ ownership. */
bool RT_startStageTransitionRasterPalettes(const u16 palettes[3][64],
    u16 footerFirst,u16 footerCount);
void RT_serviceStageTransitionRasterVInt(void);
void RT_stopStageTransitionRaster(void);
/* Diagnostic counters for deadline checks. Read from VInt or with interrupts
   disabled for a coherent snapshot. Zero fault does not prove gameplay60fps. */
const volatile RT_StageTransitionRasterStats *RT_getStageTransitionRasterStats(void);
/* Menus may acknowledge a missed split after blocking UI/audio work. The
 * next VInt already re-primes all packets; this does not change IRQ state. */
u16 RT_acknowledgeStageTransitionRasterFault(void);
#endif
