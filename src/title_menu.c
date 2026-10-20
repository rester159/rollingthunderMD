#include "rt_title_menu.h"
#include "rt_rom_layout.h"
#include "rt_stage_bank_image.h"
#include "rt_campaign_completion_scene.h"
extern RT_CompletionScene *RT_titleMenuStorage(void);
extern const u32 rtTitleLogoPatterns[];
extern const u16 rtTitleLogoMap[],rtTitleMdMap[],rtTitleLogoCount;
static u8 active;
extern const u32 rtMenuFont[768];
/* OR one 8-pixel row (nibble mask) into a chunk tile row with a pen. */
static void blend(u32 *tiles,u16 left,u16 width,u16 height,u8 spriteOrder,u16 tx,u16 y,u32 mask,u32 pens)
{
 u16 ty=y>>3;
 if(!mask || tx<left || tx>=left+width || ty>=height)return;
 tx-=left;
 u16 tile=spriteOrder?(tx>>2)*4*height+(tx&3)*height+ty:ty*width+tx;
 u32 *word=tiles+tile*8+(y&7);
 *word=(*word&~mask)|(mask&pens);
}
u32 rtMenuChunk[16*8];
/* One glyph row at an arbitrary pixel x (relative to the chunk). */
static void blendPx(u32 *tiles,u16 width,u16 height,u8 spriteOrder,s16 x,s16 y,u32 g,u32 pens)
{
 if(y<0 || x<=-8)return;
 s16 tx=x>>3;u8 shift=(x&7)*4;
 if(tx>=0)blend(tiles,0,width,height,spriteOrder,tx,y,g>>shift,pens);
 if(shift && tx+1>=0)blend(tiles,0,width,height,spriteOrder,tx+1,y,g<<(32-shift),pens);
}
u16 RT_menuTextWidth(const char *text)
{
 u16 n=0;while(text[n])n++;
 return n?n*RT_MENU_ADVANCE+1:0;
}
void RT_menuComposeOutline(u32 *tiles,s16 leftPx,u16 width,u16 height,u8 spriteOrder,
    const RT_MenuText *texts,u8 count,u8 redPen,u8 shadowPen,u8 pitch)
{
 for(u16 i=0;i<width*height*8;i++)tiles[i]=0;
 u32 red=0x11111111UL*redPen,shadow=0x11111111UL*shadowPen;
 /* Hard 1px drop shadow (down-right) first, red glyphs on top. */
 for(u8 pass=0;pass<2;pass++)for(u8 n=0;n<count;n++){
  s16 x=texts[n].x-leftPx;s16 y0=texts[n].row*pitch+1;
  for(const char *t=texts[n].text;*t;t++,x+=RT_MENU_ADVANCE){
   if(x>=(s16)(width*8)+1 || x<-9)continue;
   u8 c=*t;if(c>='a' && c<='z')c-=32;
   if(c<=32 || c>127)continue;
   const u32 *glyph=rtMenuFont+(c-32)*8;
   for(s16 r=0;r<8;r++){
    u32 g=glyph[r];
    if(pass){blendPx(tiles,width,height,spriteOrder,x,y0+r,g,red);continue;}
    blendPx(tiles,width,height,spriteOrder,x+1,y0+r+1,g,shadow);
   }
  }
 }
}
void RT_menuCompose(u32 *tiles,u16 left,u16 width,u16 height,u8 spriteOrder,
    const RT_MenuString *strings,u8 count,u8 redPen,u8 whitePen,u8 pitch)
{
 for(u16 i=0;i<width*height*8;i++)tiles[i]=0;
 u32 red=0x11111111UL*redPen,white=0x11111111UL*whitePen;
 /* White copies (2px right, 2px down) first, then red glyphs on top. */
 for(u8 pass=0;pass<2;pass++)for(u8 n=0;n<count;n++){
  u16 cx=strings[n].column,y0=strings[n].row*pitch;
  for(const char *t=strings[n].text;*t;t++,cx++){
   if(cx+1<left || cx>=left+width)continue;
   u8 c=*t;if(c>='a' && c<='z')c-=32;
   if(c<=32 || c>127)continue;
   const u32 *glyph=rtMenuFont+(c-32)*8;
   for(u16 r=0;r<8;r++){
    u32 g=glyph[r];
    if(pass)blend(tiles,left,width,height,spriteOrder,cx,y0+r,g,red);
    else{
     blend(tiles,left,width,height,spriteOrder,cx,y0+r+2,g>>8,white);
     blend(tiles,left,width,height,spriteOrder,cx+1,y0+r+2,g<<24,white);
    }
   }
  }
 }
}
/* Title canvas: 24x4 tiles (192x32px) = six 32x32 sprites at tiles 16..111. */
#define TEXT_COLUMNS 24
#define TEXT_ROWS 4
#define TEXT_SPRITES (TEXT_COLUMNS/4)
/* Lines 11px apart: 8px glyph, 1px shadow, 2px gap. The selector-0
 * console uses scene group5: PAL1 pen15 is light gray above the raster
 * split and white below it; pen1 is black / near-black. Reuse those inks
 * without changing the background, cast, sprite count or menu tile budget. */
#define TEXT_PITCH 11
#define TEXT_TOP 124
static RT_MenuString textStrings[8];
static u8 textStringCount,textCount;
static RT_SpriteCommand textSprites[TEXT_SPRITES];
extern void (*rtCompletionSatDecorate)(RT_SpriteBatch *batch);
/* Append the menu sprites after the cast. The menu rows lie above the
 * cast's sprite tops (y153), so neither overlap nor line limits change. */
static void decorate(RT_SpriteBatch *b)
{
 u16 n=b->count;
 if(!textCount || n+textCount>80)return;
 if(n)b->commands[n-1].link=n;
 for(u8 i=0;i<textCount;i++){b->commands[n+i]=textSprites[i];b->commands[n+i].link=n+i+1;}
 b->commands[n+textCount-1].link=0;b->count=n+textCount;
}
void RT_titleMenuTextClear(void){textStringCount=0;}
void RT_titleMenuTextAt(const char *text,u16 column,u16 row)
{
 if(textStringCount<8)textStrings[textStringCount++]=(RT_MenuString){text,column,row};
}
void RT_titleMenuTextCommit(void)
{
 u8 left=40,top=28;
 for(u8 i=0;i<textStringCount;i++){
  if(textStrings[i].column<left)left=textStrings[i].column;
  if(textStrings[i].row<top)top=textStrings[i].row;
 }
 /* Columns keep their relative layout at the wider letter advance; the
  * block is then centred on the screen. */
 RT_MenuText local[8];u16 contentWidth=0;
 for(u8 i=0;i<textStringCount;i++){
  s16 x=(textStrings[i].column-left)*RT_MENU_ADVANCE;
  local[i]=(RT_MenuText){textStrings[i].text,x,textStrings[i].row-top};
  u16 right=x+RT_menuTextWidth(textStrings[i].text);
  if(right>contentWidth)contentWidth=right;
 }
 /* One 32x32 sprite (16 tiles) per chunk through the shared buffer. */
 for(u8 k=0;k<TEXT_SPRITES;k++){
  RT_menuComposeOutline(rtMenuChunk,k*32,4,TEXT_ROWS,1,local,textStringCount,15,1,TEXT_PITCH);
  /* Queued copy: lands in VBlank, never mid-screen against the raster. */
  VDP_loadTileData(rtMenuChunk,16+k*16,16,DMA_QUEUE_COPY);
 }
 textCount=0;
 if(!textStringCount)return;
 u16 screenX=(320-contentWidth)/2;
 for(u8 k=0;k<TEXT_SPRITES;k++)
  textSprites[textCount++]=(RT_SpriteCommand){TEXT_TOP+128,SPRITE_SIZE(4,4),0,
   TILE_ATTR_FULL(PAL1,TRUE,FALSE,FALSE,16+k*16),screenX+k*32+128};
}
/* Tiles 320..415 are free between the logo (112..300) and scene (512+).
 * Row 27 is below the footer palette split, so use its brightest pen. */
#define FOOTER_FONT 320
static u8 footerPalette;
static void loadFooterFont(const RT_SceneBackground *b)
{
 u8 pen=1;u16 brightest=0;
 for(u8 i=1;i<64;i++){
  if(!(i&15))continue;
  u16 color=b->palettes[2][i];
  u16 brightness=(color&14)+((color>>4)&14)+((color>>8)&14);
  if(brightness>brightest){brightest=brightness;pen=i;}
 }
 footerPalette=pen>>4;pen&=15;
 for(u16 c=0;c<96;c++){
  u32 tile[8];
  for(u8 r=0;r<8;r++)tile[r]=rtMenuFont[c*8+r]&(0x11111111UL*pen);
  /* The arcade font has no lowercase glyphs. Keep the version's small v. */
  if(c=='v'-32){
   static const u32 smallV[8]={0,0,0,0x0f000f00,0x0f000f00,0x00f0f000,0x000f0000,0};
   for(u8 r=0;r<8;r++)tile[r]=smallV[r]&(0x11111111UL*pen);
  }
  VDP_loadTileData(tile,FOOTER_FONT+c,1,CPU);
 }
}
void RT_titleMenuFooter(void)
{
 const char *credit="PORTED BY RESTER159, 2026",*version="v0.9 BETA";
 for(u16 x=2;*credit;x++,credit++)VDP_setTileMapXY(BG_A,
  TILE_ATTR_FULL(footerPalette,TRUE,FALSE,FALSE,FOOTER_FONT+*credit-32),x,27);
 for(u16 x=29;*version;x++,version++)VDP_setTileMapXY(BG_A,
  TILE_ATTR_FULL(footerPalette,TRUE,FALSE,FALSE,FOOTER_FONT+*version-32),x,27);
}
/* Plane A row16 (console bezel) as drawn by the cast scene, restored with
 * the menu rows. */
static u16 bezelRow[36];
volatile u16 rtTitleMenuFault;
volatile u32 rtTitleMenuRasterRecoveries;
volatile u32 rtTitleMenuFrames;
u8 RT_titleMenuActive(void){return active;}
u8 RT_titleMenuBegin(void)
{
 if(active)return 1;
 RT_CompletionScene *s=RT_titleMenuStorage();
 VDP_setEnable(FALSE);DMA_flushQueue();
 if(!RT_stageBankSelect(RT_COMPLETION_PAGE)){rtTitleMenuFault=3;return 0;}
 if(!RT_completionEndingBegin(s,0)){RT_completionSceneEnd(s);rtTitleMenuFault=10+s->fault;return 0;}
 /* Selector0 is the original console cast, with no upper scene portraits.
  * Reuse its background, native scripts, collision and split palettes. */
 loadFooterFont(s->background);
 VDP_loadTileData(rtTitleLogoPatterns,112,rtTitleLogoCount,CPU);
 for(u16 y=0;y<12;y++)for(u16 x=0;x<36;x++)
  VDP_setTileMapXY(BG_A,rtTitleLogoMap[y*36+x],x+2,y+4);
 DMA_flushQueue();VDP_waitDMACompletion();
 for(u16 x=0;x<36;x++){
  *(vu32*)VDP_CTRL_PORT=VDP_READ_VRAM_ADDR(VDP_BG_A+(16*64+x+2)*2);
  bezelRow[x]=*(vu16*)VDP_DATA_PORT;
 }
 textCount=textStringCount=0;rtCompletionSatDecorate=decorate;
 active=1;rtTitleMenuFrames=0;return 1;
}
void RT_titleMenuRestoreText(void)
{
 const RT_SceneBackground *b=RT_titleMenuStorage()->background;
 for(u16 x=0;x<36;x++)VDP_setTileMapXY(BG_A,bezelRow[x],x+2,16);
 for(u16 y=17;y<28;y++)for(u16 x=0;x<36;x++)
  VDP_setTileMapXY(BG_A,b->maps[0][y*36+x],x+2,y);
}
u8 RT_titleMenuStep(void)
{
 if(!active)return 0;
 RT_CompletionScene *s=RT_titleMenuStorage();
 if(RT_completionSceneStep(s)!=1){rtTitleMenuFault=2;return 0;}
 rtTitleMenuFrames++;
 return 1;
}
void RT_titleMenuEnd(void)
{if(active){rtCompletionSatDecorate=0;textCount=0;RT_completionSceneEnd(RT_titleMenuStorage());active=0;}}

void RT_titleMenuSetHome(u8 home)
{
 for(u16 y=14;y<16;y++)for(u16 x=28;x<31;x++)
  VDP_setTileMapXY(BG_A,rtTitleLogoMap[(y-4)*36+x-2],x,y);
 if(home)for(u16 i=0;i<6;i++)VDP_setTileMapXY(BG_A,rtTitleMdMap[i],28+i%3,14+i/3);
}
