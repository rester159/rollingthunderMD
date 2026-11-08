#ifndef RT_DISPLAY_STREAM_RELEASE_H
#define RT_DISPLAY_STREAM_RELEASE_H
#include "rt_display_stream.h"
/* Preserve the logical row order, including its ring seam. Consecutive cells
 * owning the same physical tile can release their references as one run. */
static inline u8 RT_displayReleaseColumn(RT_DisplayStream *s,u16 plane,s16 column,s16 row)
{
    u16 *cells=s->cells+plane*2048;
    u16 index=((u16)row&31)*64+((u16)column&63);
    for(u16 i=0;i<32;){
        u16 attr=cells[index];
        if(attr==65535){i++;index=(index+64)&2047;continue;}
        u16 tile=attr&2047,count=0;
        do {cells[index]=65535;index=(index+64)&2047;i++;count++;}
        while(i<32 && cells[index]!=65535 && (cells[index]&2047)==tile);
        u16 slot=tile-s->tiles.firstTile;
        if(slot>=s->tiles.slots || s->tiles.references[slot]<count)return 0;
        u16 refs=s->tiles.references[slot]-count;
        if(refs)s->tiles.references[slot]=refs;
        else {
            s->tiles.references[slot]=1;
            if(!RT_tileCacheRelease(&s->tiles,attr))return 0;
        }
    }
    return 1;
}
#endif
