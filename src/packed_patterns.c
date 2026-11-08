/* Lossless pose reconstruction into memory owned until its DMA commits. */
#include <genesis.h>
#include "rt_rom_layout.h"
#include "rt_packed_cache.h"
#ifdef RT_PACKED_PATTERNS
extern void RT_decodeCanonicalPose(const void *stream,const void *palette,void *output,u32 bytes);
u16 rtPackedBufferHighWater;
u16 rtPackedBufferFailures;
volatile u32 rtPackedEarlyDecodes,rtPackedLateDecodes;
void RT_decodePackedPose(const void *tagged,u16 *output,u16 words)
{
    const u8 *descriptor=(const u8*)((u32)tagged&0x00ffffffUL);
    #ifdef RT_PACKED_STREAM_TABLE
    u32 encoded=*(const u32*)(RT_PACKED_STREAM_TABLE+*(const u16*)descriptor);
    u32 paletteOffset=*(const u16*)(descriptor+2);
#else
    u32 encoded=*(const u32*)descriptor;
    u32 paletteOffset=*(const u16*)(descriptor+4);
#endif
    u32 physical=encoded&0x00ffffffUL;
#ifndef RT_LINEAR_ROM
    u16 previous=SYS_getBank(7);
    SYS_setBank(7,physical>>19);
#endif

#ifdef RT_PACKED_PALETTE_WORD_OFFSETS
    paletteOffset*=2;
#endif
    const void *palette=(const void*)(RT_PACKED_PALETTE_BASE+paletteOffset);
    /* Interrupt-side banked DMA restores window 7 before returning. */
#ifdef RT_LINEAR_ROM
    RT_decodeCanonicalPose((const void*)physical,palette,output,((encoded>>24)<<16)|(u32)words*2);
#else
    RT_decodeCanonicalPose((const void*)(0x380000UL+(physical&0x7ffffUL)),palette,output,((encoded>>24)<<16)|(u32)words*2);
    SYS_setBank(7,previous);
#endif
}
#ifdef RT_LINEAR_ROM
void *RT_prepareCanonicalPose(const void *tagged,u16 words)
{
    u16 *output=RT_packedPrefetched(tagged,words);
    if(output)return output;
    output=RT_packedPrepareAllocate(words);
    if(!output)return 0;
    RT_decodePackedPose(tagged,output,words);rtPackedEarlyDecodes++;return output;
}
#endif
u16 RT_queueCanonicalPose(const void *tagged,u16 destination,u16 words)
{
    u16 *output=0;
#ifdef RT_LINEAR_ROM
    output=RT_packedPrefetched(tagged,words);
    if(output)return DMA_queueDmaFast(DMA_VRAM,output,destination,words,2);
#endif
    output=DMA_allocateTemp(words);
    if(!output){rtPackedBufferFailures++;return 0;}
    u16 used=(u8*)(output+words)-(u8*)dmaDataBuffer;
    if(used>rtPackedBufferHighWater)rtPackedBufferHighWater=used;
    RT_decodePackedPose(tagged,output,words);rtPackedLateDecodes++;
    if(!DMA_queueDmaFast(DMA_VRAM,output,destination,words,2)){
        DMA_releaseTemp(words);return 0;
    }
    return 1;
}
#endif
