#ifndef RT_TITLE_MENU_H
#define RT_TITLE_MENU_H
#include <genesis.h>
u8 RT_titleMenuActive(void);
u8 RT_titleMenuBegin(void);
void RT_titleMenuEnd(void);
u8 RT_titleMenuStep(void);
void RT_titleMenuRestoreText(void);
void RT_titleMenuSetHome(u8 home);
void RT_titleMenuFooter(void);
/* Menu text: red glyphs in front of a white copy offset 2px right/down. */
typedef struct {const char *text;u8 column,row;} RT_MenuString;
/* Compose strings (cells) into a cleared 4bpp chunk covering cell columns
 * left..left+width-1 and rows 0..height-1. spriteOrder lays tiles out as
 * 4x4 sprites (column-major inside); otherwise row-major. rtMenuChunk is a
 * shared 16-tile buffer for chunks up to 16 tiles. */
extern u32 rtMenuChunk[16*8];
void RT_menuCompose(u32 *tiles,u16 left,u16 width,u16 height,u8 spriteOrder,
    const RT_MenuString *strings,u8 count,u8 redPen,u8 whitePen,u8 pitch);
/* Menu text in the 80s arcade style: glyphs with a hard 1px drop shadow
 * down-right, 9px letter advance. x is in pixels within the canvas; line n's glyphs start at
 * y = n*pitch+1 (pitch >= 10 keeps shadows clear of the next line). leftPx is the canvas
 * pixel at this chunk's first tile column. */
typedef struct {const char *text;s16 x;u8 row;} RT_MenuText;
#define RT_MENU_ADVANCE 9
void RT_menuComposeOutline(u32 *tiles,s16 leftPx,u16 width,u16 height,u8 spriteOrder,
    const RT_MenuText *texts,u8 count,u8 redPen,u8 shadowPen,u8 pitch);
/* Pixel width of a string drawn by RT_menuComposeOutline, shadow included. */
u16 RT_menuTextWidth(const char *text);
/* Title-scene menus: collect strings, then commit composes them into tiles
 * 16..111 and shows them as sprites over the untouched background. */
void RT_titleMenuTextClear(void);
void RT_titleMenuTextAt(const char *text,u16 column,u16 row);
void RT_titleMenuTextCommit(void);
#endif
