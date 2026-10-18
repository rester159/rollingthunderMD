#ifndef RT_NATIVE_DISPLAY_H
#define RT_NATIVE_DISPLAY_H
#include "rt_world.h"
#ifndef __m68k__
typedef uint32_t u32;
#endif
/* Build-time resolved Genesis patterns and name-table attributes. Neither
 * source graphics words nor palette fitting are used by the display runtime. */
typedef struct {
    const u32 *patterns;
    const u16 *map[2];
    const u16 *palettes;
    u16 widthCells,heightCells,patternCount;
    u16 padding[2];
    u8 story,area;
    /* Optional shared16-cell blocks. map holds block indices when non-null;
     * otherwise it holds direct attributes. Dimensions must be16-cell aligned. */
    const u16 *blocks;
} RT_DisplayStage;
static inline u16 RT_displayMapCell(const RT_DisplayStage *s,u16 plane,u32 offset)
{
    return s->blocks?s->blocks[(u32)s->map[plane][offset>>4]*16+(offset&15)]:s->map[plane][offset];
}
/* Decode one incoming ring-buffer row/column from shared map blocks. */
static inline void RT_displayPackedStrip(const RT_DisplayStage *stage,u16 plane,
    s16 at,s16 start,u8 vertical,u16 *buffer)
{
    u16 count=vertical?32:64,mask=count-1;
    u16 limit=vertical?stage->heightCells:stage->widthCells;
    s16 begin=start,end=start+count;
    u16 padding=stage->padding[plane];
    if((u16)at>=(vertical?stage->widthCells:stage->heightCells)){
        for(u16 i=0;i<count;i++)buffer[i]=padding;
        return;
    }
    if(begin<0)begin=0;
    if(end>limit)end=limit;
    if(begin>=end){for(u16 i=0;i<count;i++)buffer[i]=padding;return;}
    for(s16 i=start;i<begin;i++)buffer[(u16)i&mask]=padding;
    for(s16 i=end;i<start+count;i++)buffer[(u16)i&mask]=padding;
    if(vertical){
        u16 stride=stage->widthCells>>4;
        const u16 *map=stage->map[plane]+(u32)begin*stride+((u16)at>>4);
        const u16 *blocks=stage->blocks+((u16)at&15);
        for(s16 i=begin;i<end;i++,map+=stride)
            buffer[(u16)i&31]=blocks[(u32)*map*16];
    }else{
        const u16 *map=stage->map[plane]+((u32)at*stage->widthCells+(u16)begin)/16;
        u16 position=begin;
        while(position<(u16)end){
            u16 pen=position&15,run=16-pen;
            if(run>(u16)end-position)run=(u16)end-position;
            const u16 *source=stage->blocks+(u32)*map++*16+pen;
            u16 *dest=buffer+(position&63);position+=run;
            for(u16 i=0;i<run;i++)*dest++=*source++;
        }
    }
}
typedef struct {
    const u16 *cells;
    u8 width,height;
} RT_DisplayPatch;
/* Indexed by native door kind 0..2 and restore/replacement variant 0..1.
 * Attributes reference the associated stage atlas; graphics plane 0 only. */
extern const RT_DisplayPatch rtDisplayDoorPatches[3][2];
extern const RT_DisplayStage rtDisplayOpening;
#endif
