#include "rt_segmented_pcm.h"
#include "rt_rom_layout.h"
#ifdef RT_STREAMED_PCM
#include "rt_pcm_stream_locations.h"
#endif
bool RT_planSegmentedPcm(u32 address,u32 count,u32 romBytes,RT_SegmentedPcmPlan *out)
{
 if(!out || !count || count>2097151UL || (address&32767) ||
    !romBytes || romBytes>0x2000000UL || (romBytes&0x7ffffUL) ||
    address>=romBytes)return FALSE;
 u32 stored=count;
#ifdef RT_STREAMED_PCM
 /* Sample count measures playback duration, not compressed ROM storage.
  * Admit only the exact clips verified and placed by the build. */
 stored=0;
 for(u16 i=0;i<4;i++)if(rtPcmStreamClips[i][0]==address && rtPcmStreamClips[i][1]==count)
     stored=rtPcmStreamClips[i][4];
 if(!stored || stored>romBytes-address)return FALSE;
#else
 if(count>romBytes-address || romBytes-address-count<16)return FALSE;
#endif
 u16 preload=count<256?count:256;
 u32 remaining=count-preload;
 RT_SegmentedPcmPlan p={address>>19,
#ifdef RT_LINEAR_ROM
     address>>15,
#else
     0x10+((address&0x7ffffUL)>>15),
#endif
     preload,
     remaining>>8,remaining&255,preload&255,(address+stored-1)>>19};
 *out=p;return TRUE;
}
