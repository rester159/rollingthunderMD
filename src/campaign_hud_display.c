#include "rt_campaign_hud_display.h"
#include "rt_sprites.h"
static RT_CampaignHudDisplay *owner;
volatile u8 rtHudFault;
/* Title composition and live gameplay never overlap. Reuse its 512-byte
 * scratch buffer so cold boot retains enough heap to unpack the SDK font. */
extern u32 rtMenuChunk[128];
typedef struct {RT_SpriteCommand sprites[22];u8 pens[5],layers[5];} HudScratch;
#define hudScratch ((HudScratch*)(void*)rtMenuChunk)
#define hudSprites (hudScratch->sprites)
#define inkPens (hudScratch->pens)
#define inkLayers (hudScratch->layers)
_Static_assert(sizeof(HudScratch)<=512,"HUD/title scratch overlay");
static u8 palettes[3];
/* All stage atlases share these five source color groups. White digits are
 * the common update: resolve them directly instead of scanning five entries. */
static u8 groupIndex(u8 group)
{
    if(group==252)return 4;
    if(group==232)return 2;
    if(group==3)return 0;
    return group==240?3:group==228?1:255;
}
static u16 blockTile(u8 block)
{
    if(block<9)return 1664+block*12;
    block-=9;return block<5?1772+block*4:1984+(block-5)*4;
}
static u8 findPen(const u16 *colors,u8 palette,u16 color)
{for(u8 pen=1;pen<16;pen++)if(colors[palette*16+pen]==color)return pen;return 0;}
/* The atlas has 0/1 nibbles. Shift/add coloring avoids the 68000's
 * expensive long multiply for every glyph row, including area reloads. */
static void colorGlyph(u32 *out,const u32 *source,u8 pen)
{
#define COLOR_ROWS(expression) for(u8 y=0;y<8;y++){u32 m=source[y];out[y]=(expression);}break
    switch(pen){
        case 0:for(u8 y=0;y<8;y++)out[y]=0;break;
        case 4:COLOR_ROWS(m<<2);
        case 5:COLOR_ROWS((m<<2)+m);
        case 6:COLOR_ROWS((m<<2)+(m<<1));
        case 7:COLOR_ROWS((m<<3)-m);
        case 8:COLOR_ROWS(m<<3);
        case 9:COLOR_ROWS((m<<3)+m);
        case 10:COLOR_ROWS((m<<3)+(m<<1));
        case 11:COLOR_ROWS((m<<3)+(m<<1)+m);
        case 15:COLOR_ROWS((m<<4)-m);
        default:COLOR_ROWS(m*pen);
    }
#undef COLOR_ROWS
}
static u8 renderBlock(RT_CampaignHudDisplay *d,const RT_CampaignHud *hud,u8 block,TransferMethod method)
{
    u32 patterns[96];
    u8 bottom=block>=9;
    u8 column=bottom?(block-9)*4:block*4,height=bottom?1:3;
    for(u8 x=0;x<4;x++)for(u8 y=0;y<height;y++){
        RT_HudCell cell=hud->cells[(bottom?3:y)*36+column+x];
        u8 group=groupIndex(cell.group);
        if(group==255)return 0;
        u16 attr=d->art->attributes[group][cell.glyph];if(attr==65535)return 0;
        const u32 *source=d->art->patterns+(attr&2047)*8;
        u8 pen=inkPens[group];
        colorGlyph(patterns+(x*height+y)*8,source,pen);
    }
    return DMA_transfer(method,DMA_VRAM,patterns,blockTile(block)*32,height*4*16,2);
}
static u8 bottomSprites(RT_CampaignHudDisplay *d,const RT_CampaignHud *hud)
{
    u8 pens[36];
    for(u8 x=0;x<36;x++){
        u8 group=groupIndex(hud->cells[108+x].group);
        if(group==255)return 0;
        pens[x]=palettes[inkLayers[group]];
    }
    u8 n=9;
    for(u8 x=0;x<36;){
        u8 width=1;
        while(width<4&&x+width<36&&x+width!=20&&pens[x+width]==pens[x])width++;
        if(n>=21)return 0;
        hudSprites[n++]=(RT_SpriteCommand){344,(width-1)*4,0,
            0x8000|(pens[x]<<13)|(x<20?1772+x:1984+x-20),(u16)(144+x*8)};
        x+=width;
    }
    while(n<22)hudSprites[n++]=(RT_SpriteCommand){0,0,0,0,128};
    return 1;
}
static u8 uploadTail(TransferMethod method)
{
    for(u8 i=4;i<21;i++)hudSprites[i].link=59+i;
    hudSprites[21].link=4;
    return DMA_transfer(method,DMA_VRAM,hudSprites+4,0xf400+62*8,18*4,2);
}
u8 RT_campaignHudDisplayBegin(RT_CampaignHudDisplay *d,const RT_CampaignHudArt *art,u16 first)
{
    rtHudFault=1;
    if(!d||owner||!art||DMA_getQueueSize()||(VDP_getReg(1)&0x40))return 0;
    if(art->groupCount!=5)return 0;
    for(u8 i=0;i<5;i++)if(groupIndex(art->groups[i])!=i)return 0;
    rtHudFault=2;
    u16 colors[64];PAL_getColors(0,colors,64);
    /* Top uses white/red. Bottom adds health green and timer yellow; split
     * exactly across two existing stage palettes, never recolor the world. */
    u8 top=0;while(top<4&&(!findPen(colors,top,0xccc)||!findPen(colors,top,0x00e)||!findPen(colors,top,0x0ee)))top++;
    if(top==4)return 0;rtHudFault=3;palettes[0]=top;palettes[1]=top;
    u16 greenColor=colors[art->inks[2]];
    u8 second=0;while(second<4&&!findPen(colors,second,greenColor))second++;
    if(second==4)return 0;palettes[2]=second;
    for(u8 group=0;group<5;group++){
        u16 color=colors[art->inks[group]];
        u8 pen=group?findPen(colors,top,color):0,layer=1;
        if(group&&!pen){layer=2;pen=findPen(colors,second,color);}
        if(group&&!pen)return 0;
        inkPens[group]=pen;inkLayers[group]=layer;
    }
    d->art=art;d->firstTile=first;d->ready=0;
    memcpy(d->previous.cells,art->initial,sizeof d->previous);
    for(u8 block=0;block<18;block++){
        if(block<9)hudSprites[block]=(RT_SpriteCommand){128,14,0,
            0x8000|(palettes[0]<<13)|blockTile(block),(u16)(144+block*32)};
        if(!renderBlock(d,&d->previous,block,CPU))return 0;
    }
    if(!bottomSprites(d,&d->previous)||!uploadTail(CPU))return 0;
    hudSprites[0].link=1;hudSprites[1].link=2;hudSprites[2].link=3;hudSprites[3].link=62;
    extern void RT_spritesInitializeHud(const RT_SpriteCommand*);
    RT_spritesInitializeHud(hudSprites);
    DMA_transfer(CPU,DMA_VRAM,hudSprites,0xf400,16,2);
    /* The fixed tail already links to entry four. Terminate there until the
     * first actor publication so an area load cannot expose the previous SAT. */
    RT_SpriteCommand empty={0,0,0,0,128};
    DMA_transfer(CPU,DMA_VRAM,&empty,0xf400+4*8,4,2);
    VDP_setWindowVPos(FALSE,0);VDP_setHInterrupt(FALSE);
    owner=d;d->ready=1;rtHudFault=0;return 1;
}
u8 RT_campaignHudDisplayQueue(RT_CampaignHudDisplay *d,const RT_CampaignHud *hud)
{return RT_campaignHudDisplayQueueRows(d,hud,15);}
u8 RT_campaignHudDisplayQueueRows(RT_CampaignHudDisplay *d,const RT_CampaignHud *hud,u8 dirtyRows)
{
    if(!d||!hud||owner!=d||!d->ready||dirtyRows>15)return 0;
    if(!dirtyRows)return 1;
    u8 recolor=0;
    for(u8 row=0;row<4;row++){
        if(!(dirtyRows&(1<<row)))continue;
        for(u8 x=0;x<36;x++){
            u16 index=row*36+x;RT_HudCell cell=hud->cells[index],old=d->previous.cells[index];
            if(cell.glyph==old.glyph&&cell.group==old.group)continue;
            u8 group=groupIndex(cell.group);
            if(group==255)return 0;
            u16 attr=d->art->attributes[group][cell.glyph];if(attr==65535)return 0;
            const u32 *source=d->art->patterns+(attr&2047)*8;
            u32 pixels[8];u8 pen=inkPens[group];
            colorGlyph(pixels,source,pen);
            u16 tile=row<3?1664+(x/4)*12+(x%4)*3+row:x<20?1772+x:1984+x-20;
            if(!DMA_transfer(DMA_QUEUE_COPY,DMA_VRAM,pixels,tile*32,16,2))return 0;
            if(row==3&&cell.group!=old.group)recolor=1;
            d->previous.cells[index]=cell;
        }
    }
    if(recolor&&(!bottomSprites(d,hud)||!uploadTail(DMA_QUEUE_COPY)))return 0;
    return 1;
}
void RT_campaignHudDisplayEnd(RT_CampaignHudDisplay *d)
{if(d&&owner==d){owner=0;d->ready=0;VDP_setWindowVPos(FALSE,0);}}
