/* Validate generated placement with the actual runtime admission functions. */
#include <genesis.h>
#include <assert.h>
#include <stdio.h>
#include "rt_rom_layout.h"
#include "rt_final_audio_locations.h"
#include "rt_pcm_rom_plan.h"
#include "rt_segmented_pcm.h"
int main(void)
{
    RT_PcmRomPlan windows;
    RT_SegmentedPcmPlan segmented;
    assert(RT_planPcmRomClipWindows(RT_FINAL_VICTORY_ADDRESS,
        RT_FINAL_VICTORY_SAMPLES,RT_ROM_BYTES,RT_FINAL_VICTORY_WINDOWS,&windows));
    assert(RT_planSegmentedPcm(RT_FINAL_VICTORY_ADDRESS,RT_FINAL_VICTORY_SAMPLES,RT_ROM_BYTES,&segmented));
    assert(RT_planPcmRomClipWindows(RT_FINAL_ENDING_AFTER_VICTORY_ADDRESS,
        RT_FINAL_ENDING_AFTER_VICTORY_SAMPLES,RT_ROM_BYTES,2,&windows));
    assert(RT_planSegmentedPcm(RT_FINAL_ENDING_AFTER_VICTORY_ADDRESS,
        RT_FINAL_ENDING_AFTER_VICTORY_SAMPLES,RT_ROM_BYTES,&segmented));
    assert(RT_planSegmentedPcm(RT_HIGH_COLD_ADDRESS,1118805,RT_ROM_BYTES,&segmented));
    assert(RT_planSegmentedPcm(RT_HIGH_WARM_ADDRESS,1118805,RT_ROM_BYTES,&segmented));
#ifdef RT_STREAMED_PCM
    /* Compressed span fits even when its decoded sample count would not.
     * Unknown descriptors and truncated physical storage must still fail. */
    assert(!RT_planSegmentedPcm(RT_HIGH_WARM_ADDRESS,1118804,RT_ROM_BYTES,&segmented));
    assert(!RT_planSegmentedPcm(RT_HIGH_WARM_ADDRESS+32768,1118805,RT_ROM_BYTES,&segmented));
    assert(!RT_planSegmentedPcm(RT_HIGH_WARM_ADDRESS,1118805,RT_HIGH_WARM_ADDRESS&~0x7ffffUL,&segmented));
#endif
    puts("All four finite PCM clips pass the runtime alignment/span admission checks.");
    return 0;
}
