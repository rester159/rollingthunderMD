/* Experimental compressed-layout path. All outputs use the existing DMA arena
 * and survive until its queue is flushed. Ordinary raw layouts retain their
 * original upload path. The build verifies every restored pixel against the
 * frozen source; streams are bounded within one mapper page. */
#include <genesis.h>
#include "rt_rom_layout.h"
#ifdef RT_PACKED_PATTERNS
extern void RT_decodeCanonicalPose(const void *stream,const void *palette,void *output,u32 bytes);
u16 rtPackedBufferHighWater;
u16 rtPackedBufferFailures;
u16 RT_queueCanonicalPose(const void *tagged,u16 destination,u16 words)
{
    const u8 *descriptor=(const u8*)((u32)tagged&0x00ffffffUL);
    u32 encoded=*(const u32*)descriptor;
    u32 physical=encoded&0x00ffffffUL;
    u16 *output=DMA_allocateTemp(words);
    if(!output){rtPackedBufferFailures++;return 0;}
    u16 used=(u8*)(output+words)-(u8*)dmaDataBuffer;
    if(used>rtPackedBufferHighWater)rtPackedBufferHighWater=used;
    /* Only window 7 is transient. Interrupt-side banked DMA restores it;
     * no interrupt handler accesses these immutable compressed sources. */
    u16 previous=SYS_getBank(7);
    SYS_setBank(7,physical>>19);
    const void *palette=(const void*)(RT_PACKED_PALETTE_BASE+*(const u16*)(descriptor+4));
    RT_decodeCanonicalPose((const void*)(0x380000UL+(physical&0x7ffffUL)),palette,output,((encoded>>24)<<16)|(u32)words*2);
    SYS_setBank(7,previous);
    if(!DMA_queueDmaFast(DMA_VRAM,output,destination,words,2)){
        DMA_releaseTemp(words);return 0;
    }
    return 1;
}
#endif
