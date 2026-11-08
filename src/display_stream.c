#include <genesis.h>
#include "rt_display_stream.h"
#include "rt_display_stream_release.h"
typedef struct {RT_DisplayRuntime *display;TransferMethod method;} Upload;
static u8 upload(u16 pattern,u16 tile,void *context)
{
    Upload *u=context;
    if(!DMA_transfer(u->method==CPU?CPU:DMA_QUEUE,DMA_VRAM,
        (void*)(u->display->stage->patterns+(u32)pattern*8),tile*32,16,2))return 0;
    if(u->method!=CPU)u->display->queuedBytes+=32;
    return 1;
}
static u16 indexAt(u16 p,s16 x,s16 y){return p*2048+((u16)y&31)*64+((u16)x&63);}
static u8 release(RT_DisplayStream *s,u16 index)
{
    if(s->cells[index]==65535)return 1;
    if(!RT_tileCacheRelease(&s->tiles,s->cells[index]))return 0;
    s->cells[index]=65535;return 1;
}
/* An outgoing row contains every physical column once. Aggregate runs of
 * the same physical tile instead of updating its reference count64 times. */
static u8 releaseRow(RT_DisplayStream *s,u16 p,s16 row)
{
    u16 *cells=s->cells+p*2048+((u16)row&31)*64;
    for(u16 i=0;i<64;){
        u16 attr=cells[i];
        if(attr==65535){i++;continue;}
        u16 tile=attr&2047,count=0;
        do {cells[i++]=65535;count++;}
        while(i<64 && cells[i]!=65535 && (cells[i]&2047)==tile);
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
static u8 map(RT_DisplayStream *s,Upload *u,u16 index,u16 attr,u16 *mapped)
{
    if(s->cells[index]!=65535 && !release(s,index))return 0;
    /* Most strip cells refer to a tile still owned elsewhere in the ring.
     * That path changes no free-list state and needs no upload callback. */
    u16 pattern=attr&2047;
    if(pattern<s->tiles.patternCount){
        u16 cached=s->tiles.mapping[pattern];
        if(cached){
            u16 slot=cached-1,refs=s->tiles.references[slot];
            if(refs && refs<65535){
                s->tiles.references[slot]=refs+1;
                *mapped=(attr&0xf800)|(s->tiles.firstTile+slot);
                s->cells[index]=*mapped;return 1;
            }
        }
    }
    if(!RT_tileCacheAcquire(&s->tiles,attr,mapped,upload,u))return 0;
    s->cells[index]=*mapped;return 1;
}
/* Each strip walks a ROM span. Compute its row product once, rather than
 * repeating a world-cell lookup/multiplication for every incoming tile. */
static u8 stripPacked(RT_DisplayStream *s,RT_DisplayRuntime *d,s16 at,u8 vertical,TransferMethod method)
{
    u16 buffer[64];Upload u={d,method};
    const RT_DisplayStage *stage=d->stage;
    u16 count=vertical?32:64;
    s16 start=vertical?d->row:d->column;
    u16 fixed=vertical?((u16)at&63):((u16)at&31)*64;
    for(u16 p=0;p<2;p++){
        /* Resolve the immutable source strip first. The map/ownership pass
         * keeps its original logical order and reuses the same output buffer. */
        RT_displayPackedStrip(stage,p,at,start,vertical,buffer);
        u16 base=p*2048+fixed;
        for(u16 i=0;i<count;i++){
            u16 index=(u16)(start+i)&(vertical?31:63);
            u16 destination=base+(vertical?index*64:index);
            if(!map(s,&u,destination,buffer[index],buffer+index))return 0;
        }
        u16 address=(p?0xe000:0xc000)+(vertical?((u16)at&63)*2:((u16)at&31)*128);
        if(!DMA_transfer(method,DMA_VRAM,buffer,address,count,vertical?128:2))return 0;
        if(method!=CPU)d->queuedBytes+=count*2;
    }
    return 1;
}
static u8 strip(RT_DisplayStream *s,RT_DisplayRuntime *d,s16 at,u8 vertical,TransferMethod method)
{
    if(d->stage->blocks)return stripPacked(s,d,at,vertical,method);
    u16 buffer[64];Upload u={d,method};
    const RT_DisplayStage *stage=d->stage;
    u16 count=vertical?32:64,limit=vertical?stage->heightCells:stage->widthCells;
    u16 step=vertical?stage->widthCells:1;
    s16 start=vertical?d->row:d->column;
    u8 fixedValid=(u16)at<(vertical?stage->widthCells:stage->heightCells);
    s32 first=vertical?(s32)start*stage->widthCells+at:(s32)at*stage->widthCells+start;
    u16 fixed=vertical?((u16)at&63):((u16)at&31)*64;
    for(u16 p=0;p<2;p++){
        const u16 *source=stage->map[p];s32 offset=first;
        u16 base=p*2048+fixed,padding=stage->padding[p];
        for(u16 i=0;i<count;i++,offset+=step){
            s16 coordinate=start+i;
            u16 index=(u16)coordinate&(vertical?31:63);
            u16 attr=fixedValid && (u16)coordinate<limit?source[offset]:padding;
            u16 destination=base+(vertical?index*64:index);
            if(!map(s,&u,destination,attr,buffer+index))return 0;
        }
        u16 address=(p?0xe000:0xc000)+(vertical?((u16)at&63)*2:((u16)at&31)*128);
        if(!DMA_transfer(method,DMA_VRAM,buffer,address,count,vertical?128:2))return 0;
        if(method!=CPU)d->queuedBytes+=count*2;
    }
    return 1;
}
u8 RT_displayStreamInitialize(RT_DisplayStream *s,RT_DisplayRuntime *d,
    const RT_DisplayStage *stage,const RT_WorldCamera *camera,u16 slots)
{
    if(!s || !stage || !RT_tileCacheInitialize(&s->tiles,stage->patternCount,0,slots))return 0;
    s->resident=stage->patternCount<=slots;
    if(s->resident)return RT_displayInitialize(d,stage,camera);
    if(!RT_displayConfigure(d,stage,camera))return 0;
    for(u16 i=0;i<4096;i++)s->cells[i]=65535;
    for(u16 y=0;y<32;y++)if(!strip(s,d,d->row+y,0,CPU))return 0;
    d->ready=1;RT_displayCommit(d);VDP_setEnable(TRUE);return 1;
}
static s16 floorCell(s16 x){return x/8-(x<0 && ((u16)x&7));}
u8 RT_displayStreamPrepare(RT_DisplayStream *s,RT_DisplayRuntime *d,const RT_WorldCamera *camera)
{
    if(!s || !d || !d->ready || !camera)return 0;
    if(s->resident)return RT_displayPrepare(d,camera);
    s16 sx=camera->x-16,sy=camera->y+32,x=floorCell(sx),y=floorCell(sy);
    s16 ox=d->column,oy=d->row,dx=x-ox,dy=y-oy;
    if(dx < -2 || dx>2 || dy < -2 || dy>2)return 0;
    d->queuedBytes=0;
    /* Retire the complete outgoing strips before any incoming acquisition.
     * Overlapping corners release once via the vacant-cell sentinel. */
    for(u16 p=0;p<2;p++){
        for(s16 i=0;i<(dx<0?-dx:dx);i++){
            s16 col=dx>0?ox+i:ox+63-i;
            if(!RT_displayReleaseColumn(s,p,col,oy))goto fail;
        }
        for(s16 i=0;i<(dy<0?-dy:dy);i++){
            s16 row=dy>0?oy+i:oy+31-i;
            if(!releaseRow(s,p,row))goto fail;
        }
    }
    d->column=x;d->row=y;
    for(s16 i=0;i<(dx<0?-dx:dx);i++)
        if(!strip(s,d,dx>0?x+64-dx+i:x-dx-1-i,1,DMA_QUEUE_COPY))goto fail;
    for(s16 i=0;i<(dy<0?-dy:dy);i++)
        if(!strip(s,d,dy>0?y+32-dy+i:y-dy-1-i,0,DMA_QUEUE_COPY))goto fail;
    d->scrollX=sx;d->scrollY=sy;return 1;
fail:d->ready=0;return 0;
}
u8 RT_displayStreamPatch(RT_DisplayStream *s,RT_DisplayRuntime *d,
    const RT_DisplayPatch *patch,s16 col,s16 row)
{
    if(s && s->resident)return RT_displayPatch(d,patch,col,row);
    if(!s || !d || !d->ready || !patch || !patch->cells || !patch->width ||
       patch->width>64 || !patch->height || patch->height>32)return 0;
    s32 left=col,top=row,right=left+patch->width,bottom=top+patch->height;
    if(left<d->column)left=d->column;
    if(top<d->row)top=d->row;
    if(right>d->column+64)right=d->column+64;
    if(bottom>d->row+32)bottom=d->row+32;
    if(left>=right || top>=bottom)return 1;
    for(s32 y=top;y<bottom;y++)for(s32 x=left;x<right;x++)
        if(!release(s,indexAt(0,x,y)))goto fail;
    Upload u={d,DMA_QUEUE};u16 buffer[64];
    for(s32 y=top;y<bottom;y++)for(s32 x=left;x<right;){
        u16 count=right-x,seam=64-((u16)x&63);if(count>seam)count=seam;
        for(u16 i=0;i<count;i++)if(!map(s,&u,indexAt(0,x+i,y),
            patch->cells[(y-row)*patch->width+x+i-col],buffer+i))goto fail;
        if(!DMA_transfer(DMA_QUEUE_COPY,DMA_VRAM,buffer,0xc000+((u16)y&31)*128+((u16)x&63)*2,count,2))goto fail;
        d->queuedBytes+=count*2;x+=count;
    }
    return 1;
fail:d->ready=0;return 0;
}
