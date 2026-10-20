#include "rt_campaign_scene_background.h"
#include "rt_rom_layout.h"
#ifdef RT_LINEAR_ROM
extern void RT_decodeCanonicalPose(const void*,const void*,void*,u32);
extern void *RT_sceneDecodeWorkspace(void);
#endif
#ifdef __m68k__
u8 RT_sceneBackgroundUploadIO(const RT_SceneBackground *b,void (*beginIO)(void),void (*endIO)(void))
{
    if((!beginIO != !endIO) || !b || !b->patterns || !b->maps || !b->palettes || !b->tileCount ||
       b->tileCount>768 || b->footerCount==1 || b->footerCount>3 ||
       (!b->footerCount && b->footerFirst) ||
       b->footerFirst>64-b->footerCount || SPR_isInitialized() || DMA_getQueueSize())return 0;
    if(beginIO)beginIO();
    VDP_setEnable(FALSE);
    /* Publish final SDK layout first; intermediate setters otherwise relocate
     * its default font into this scene's exclusively owned pattern memory. */
    SYS_disableInts();
    bga_addr=0xc000;bgb_addr=0xe000;window_addr=0xb000;
    slist_addr=0xf000;hscrl_addr=0xf400;maps_addr=0xb000;
    userTileMaxIndex=TILE_FONT_INDEX-spriteVramSize;
    VDP_setScreenWidth320();VDP_setScreenHeight224();VDP_setPlaneSize(64,32,FALSE);
    VDP_setBGAAddress(0xc000);VDP_setBGBAddress(0xe000);VDP_setWindowAddress(0xb000);
    VDP_setSpriteListAddress(0xf000);VDP_setHScrollTableAddress(0xf400);
    VDP_setWindowHPos(FALSE,0);VDP_setWindowVPos(FALSE,0);
    VDP_setScrollingMode(HSCROLL_PLANE,VSCROLL_PLANE);
    VDP_setHorizontalScroll(BG_A,0);VDP_setHorizontalScroll(BG_B,0);
    VDP_setVerticalScroll(BG_A,0);VDP_setVerticalScroll(BG_B,0);
    SYS_enableInts();
    static const u32 blank[8]={0};
    VDP_loadTileData(blank,0,1,DMA);
    if(endIO)endIO();
    for(u8 plane=0;plane<2;plane++){
        if(beginIO)beginIO();
        VDP_clearPlane(plane?BG_B:BG_A,TRUE);
        if(endIO)endIO();
    }
#ifdef RT_LINEAR_ROM
    const u8 *packed=((u32)b->patterns&0xff000000UL)==0x86000000UL?
        (const u8*)((u32)b->patterns&0xffffffUL):0;
    static const u8 identity[8]={0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef};
    u32 *decoded=RT_sceneDecodeWorkspace();
#endif
    for(u16 tile=0;tile<b->tileCount;){
        u16 count=b->tileCount-tile;if(count>32)count=32;
        const u32 *source=b->patterns+(u32)tile*8;
#ifdef RT_LINEAR_ROM
        if(packed){
            u16 length=*(const u16*)packed;packed+=2;
            RT_decodeCanonicalPose(packed,identity,decoded,0x100000UL+(u32)count*32);
            packed+=length+(length&1);source=decoded;
        }
#endif
        if(beginIO)beginIO();
        VDP_loadTileData(source,512+tile,count,DMA);
        if(endIO)endIO();tile+=count;
    }
    for(u8 plane=0;plane<2;plane++)for(u16 row=0;row<28;row+=4){
        if(beginIO)beginIO();
        VDP_setTileMapDataRect(plane?BG_B:BG_A,b->maps[plane]+row*36,2,row,36,4,36,DMA);
        if(endIO)endIO();
    }
    if(beginIO)beginIO();
    u8 ok=DMA_transfer(CPU,DMA_VRAM,(void*)blank,0xf000,4,2);
    PAL_setColors(0,b->palettes[0],64,CPU);VDP_setBackgroundColor(0);
    if(endIO)endIO();
    if(!ok)return 0;
    return 1;
}
u8 RT_sceneBackgroundUpload(const RT_SceneBackground *b)
{return RT_sceneBackgroundUploadIO(b,0,0);}
#endif
