#include <genesis.h>
#include "rt_display_runtime.h"

#define PLANE_A 0xc000
#define PLANE_B 0xe000
#define WINDOW 0xd000
#define HSCROLL 0xf000
#define SAT 0xf400

static s16 cellFloor(s16 value)
{
    return value/8-(value<0 && ((u16)value&7));
}
/* Compact maps are arrays of 16-cell blocks. Hold the current block across
 * a row run, or advance one block per vertical cell, instead of resolving
 * the same block pointer separately for every name-table attribute. */
static u8 stripPacked(const RT_DisplayRuntime *display,s16 at,u8 vertical,TransferMethod method)
{
    u16 buffer[64];const RT_DisplayStage *stage=display->stage;
    u16 count=vertical?32:64;
    s16 start=vertical?display->row:display->column;
    for(u16 plane=0;plane<2;plane++){
        RT_displayPackedStrip(stage,plane,at,start,vertical,buffer);
        u16 address=(plane?PLANE_B:PLANE_A)+(vertical?((u16)at&63)*2:((u16)at&31)*128);
        if(!DMA_transfer(method,DMA_VRAM,buffer,address,count,vertical?128:2))return 0;
    }
    return 1;
}
static u8 strip(const RT_DisplayRuntime *display,s16 at,u8 vertical,TransferMethod method)
{
    if(display->stage->blocks)return stripPacked(display,at,vertical,method);
    u16 buffer[64];const RT_DisplayStage *stage=display->stage;
    u16 count=vertical?32:64,limit=vertical?stage->heightCells:stage->widthCells;
    s16 start=vertical?display->row:display->column;
    u8 fixedValid=(u16)at<(vertical?stage->widthCells:stage->heightCells);
    s32 first=vertical?(s32)start*stage->widthCells+at:(s32)at*stage->widthCells+start;
    u16 step=vertical?stage->widthCells:1;
    for(u16 plane=0;plane<2;plane++){
        const u16 *source=stage->map[plane];s32 offset=first;
        for(u16 i=0;i<count;i++,offset+=step){
            s16 coordinate=start+i;u16 attr=stage->padding[plane];
            if(fixedValid && (u16)coordinate<limit)
                attr=stage->blocks?RT_displayMapCell(stage,plane,offset):source[offset];
            buffer[(u16)coordinate&(vertical?31:63)]=attr;
        }
        u16 address=(plane?PLANE_B:PLANE_A)+(vertical?((u16)at&63)*2:((u16)at&31)*128);
        if(!DMA_transfer(method,DMA_VRAM,buffer,address,count,vertical?128:2))return 0;
    }
    return 1;
}
static u8 row(const RT_DisplayRuntime *display,s16 y,TransferMethod method)
{return strip(display,y,0,method);}
static u8 column(const RT_DisplayRuntime *display,s16 x,TransferMethod method)
{return strip(display,x,1,method);}
void RT_displayCommit(const RT_DisplayRuntime *display)
{
    if(!display || !display->ready)return;
    VDP_setHorizontalScroll(BG_A,-display->scrollX);
    VDP_setHorizontalScroll(BG_B,-display->scrollX);
    VDP_setVerticalScroll(BG_A,display->scrollY);
    VDP_setVerticalScroll(BG_B,display->scrollY);
}
u8 RT_displayConfigure(RT_DisplayRuntime *display,const RT_DisplayStage *stage,
    const RT_WorldCamera *camera)
{
    if(!display || !stage || !camera || !stage->patterns || !stage->map[0] ||
       !stage->map[1] || !stage->palettes || !stage->widthCells ||
       !stage->heightCells || !stage->patternCount || stage->patternCount>1536 ||
       (stage->blocks && (stage->widthCells&15)))return 0;
    display->ready=0;display->stage=stage;
    /* Source logical terrain window -> H40 display crop, derived from
     * A5C7 initial map fill and the CUS43 generator viewport offsets. */
    display->scrollX=camera->x-16;display->scrollY=camera->y+32;
    display->column=cellFloor(display->scrollX);display->row=cellFloor(display->scrollY);
    display->queuedBytes=0;
    DMA_flushQueue();VDP_setEnable(FALSE);
    VDP_setScreenWidth320();VDP_setScreenHeight224();
    VDP_setPlaneSize(64,32,FALSE);
    VDP_setBGAAddress(PLANE_A);VDP_setBGBAddress(PLANE_B);
    VDP_setWindowAddress(WINDOW);VDP_setWindowHPos(FALSE,0);VDP_setWindowVPos(FALSE,0);
    VDP_setHScrollTableAddress(HSCROLL);VDP_setSpriteListAddress(SAT);
    VDP_setScrollingMode(HSCROLL_PLANE,VSCROLL_PLANE);
    VDP_resetSprites();VDP_updateSprites(0,CPU);
    PAL_setColors(0,stage->palettes,64,CPU);VDP_setBackgroundColor(0);
    return 1;
}
u8 RT_displayInitialize(RT_DisplayRuntime *display,const RT_DisplayStage *stage,
    const RT_WorldCamera *camera)
{
    if(!RT_displayConfigure(display,stage,camera))return 0;
    VDP_loadTileData(stage->patterns,0,stage->patternCount,CPU);
    for(u16 y=0;y<32;y++)if(!row(display,display->row+y,CPU))return 0;
    display->ready=1;RT_displayCommit(display);VDP_setEnable(TRUE);
    return 1;
}
u8 RT_displayPrepare(RT_DisplayRuntime *display,const RT_WorldCamera *camera)
{
    if(!display || !display->ready || !camera)return 0;
    s16 sx=camera->x-16,sy=camera->y+32;
    s16 x=cellFloor(sx),y=cellFloor(sy);
    s16 dx=x-display->column,dy=y-display->row;
    if(dx < -2 || dx>2 || dy < -2 || dy>2)return 0;
    display->queuedBytes=0;
    display->column=x;display->row=y;
    for(s16 i=0;i<(dx<0?-dx:dx);i++){
        s16 entering=dx>0?x+64-dx+i:x-dx-1-i;
        if(!column(display,entering,DMA_QUEUE_COPY)){display->ready=0;return 0;}
        display->queuedBytes+=128;
    }
    for(s16 i=0;i<(dy<0?-dy:dy);i++){
        s16 entering=dy>0?y+32-dy+i:y-dy-1-i;
        if(!row(display,entering,DMA_QUEUE_COPY)){display->ready=0;return 0;}
        display->queuedBytes+=256;
    }
    display->scrollX=sx;display->scrollY=sy;
    return 1;
}

u8 RT_displayPatch(RT_DisplayRuntime *display,const RT_DisplayPatch *patch,
    s16 worldColumn,s16 worldRow)
{
    if(!display || !display->ready || !patch || !patch->cells ||
       !patch->width || patch->width>64 || !patch->height || patch->height>32)return 0;
    /* Clip against the actual resident window before taking ring coordinates.
     * Otherwise a distant door could overwrite unrelated visible terrain. */
    s32 left=worldColumn,top=worldRow;
    s32 right=left+patch->width,bottom=top+patch->height;
    if(left<display->column)left=display->column;
    if(top<display->row)top=display->row;
    if(right>(s32)display->column+64)right=(s32)display->column+64;
    if(bottom>(s32)display->row+32)bottom=(s32)display->row+32;
    if(left>=right || top>=bottom)return 1;
    /* Immutable ROM rows need no copy buffer. Split a row at the physical
     * 64-cell seam; the VDP does not wrap a DMA at a name-table row boundary. */
    for(s32 y=top;y<bottom;y++){
        s32 x=left;
        while(x<right){
            u16 column=(u16)x&63;
            u16 count=(u16)(right-x);
            if(count>64-column)count=64-column;
            const u16 *cells=patch->cells+(y-worldRow)*patch->width+(x-worldColumn);
            if(!DMA_transfer(DMA_QUEUE,DMA_VRAM,(void*)cells,
                PLANE_A+((u16)y&31)*128+column*2,count,2)){
                display->ready=0;return 0;
            }
            display->queuedBytes+=count*2;x+=count;
        }
    }
    return 1;
}
