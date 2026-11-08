#include "rt_pcm_rom_plan.h"
#include "rt_rom_layout.h"
#ifdef RT_STREAMED_PCM
#include "rt_segmented_pcm.h"
#endif

bool RT_planPcmRomClip(u32 physicalAddress, u32 sampleCount, u32 romBytes,
                       RT_PcmRomPlan *output)
{
    return RT_planPcmRomClipWindows(physicalAddress,sampleCount,romBytes,1,output);
}

bool RT_planPcmRomClipWindows(u32 physicalAddress,u32 sampleCount,u32 romBytes,
    u16 windows,RT_PcmRomPlan *output)
{
    RT_PcmRomPlan plan;
    u32 offset = physicalAddress & (RT_PCM_ROM_BANK_BYTES - 1);
#ifdef RT_STREAMED_PCM
    RT_SegmentedPcmPlan streamed;
    if(!output || !windows || windows>2 || sampleCount>1048575UL ||
       !RT_planSegmentedPcm(physicalAddress,sampleCount,romBytes,&streamed))return FALSE;
#else
    if (!output || !windows || windows>2 || !sampleCount || sampleCount > 1048575UL ||
        (physicalAddress & 0x7FFFUL) || romBytes > 0x2000000UL ||
        (physicalAddress>>19)+windows>64 ||
        physicalAddress >= romBytes ||
        sampleCount > romBytes - physicalAddress ||
        romBytes - physicalAddress - sampleCount < RT_PCM_ROM_GUARD_BYTES ||
        sampleCount > RT_PCM_ROM_BANK_BYTES*windows - offset - RT_PCM_ROM_GUARD_BYTES)
        return FALSE;
#endif
    plan.mapperBank = physicalAddress >> 19;
    plan.z80Bank = (RT_PCM_ROM_WINDOW_BASE + offset) >> 15;
    plan.samplePointer = 0x8000;
    plan.initialRemaining = sampleCount < 16 ? sampleCount : 0xFFFF;
    plan.fullGroups = sampleCount >> 4;
    plan.tailRemaining = 13 + (sampleCount & 15);
    plan.sampleCount = sampleCount;
    *output = plan;
    return TRUE;
}
