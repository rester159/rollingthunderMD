#ifndef RT_GAME_OVER_PANEL_UPLOAD_H
#define RT_GAME_OVER_PANEL_UPLOAD_H
#include <genesis.h>
typedef struct {
 const u8 *scene,*prepared;
 u32 romBytes,lookup,records;
 u16 count,index,entries[256];
 u8 cells[672];
 bool valid,ready,sharedLookup;
} RT_GameOverPanelUpload;
typedef enum {RT_COMPOSITE_PANEL_INVALID,RT_COMPOSITE_PANEL_RETRY,
 RT_COMPOSITE_PANEL_MORE,RT_COMPOSITE_PANEL_QUEUED} RT_CompositePanelResult;
/* Bind immutable RTGC and RTPC v1/v2; no VDP writes. Initialize state to zero.
   Exclusive scene owner; do not run the head/HUD panel writer concurrently.
   This validation can scan the embedded scene: perform before timed playback.
   romBytes=0 uses FAR, otherwise resource reads reserve audio regions5/6. */
bool RT_beginGameOverPanelUpload(RT_GameOverPanelUpload *state,
 const u8 *scene,u32 sceneLength,const u8 *prepared,u32 preparedLength,u32 romBytes);
/* Hide the SAT then preload exact tile pairs at256..511 using copied DMA.
   ready requires all accepted transfers to have been committed by the owner. */
bool RT_queueGameOverPanelPatterns(RT_GameOverPanelUpload *state,u16 budgetBytes);
/* Keep panel stable through QUEUED and actual DMA commit. Start cursor at0
   for each revision. Changed cells atomically enqueue both plane entries;
   accepted cells alone update cache/cursor. Adjacent changed cells in one row
   share one copied transfer per plane. Budget includes existing DMA.
   FF00 restores both background maps. To release head VRAM, submit a complete
   FF00 panel, wait for QUEUED and actual commit, then invalidate this state
   before loading head patterns. No source timer/phase acknowledgement here. */
RT_CompositePanelResult RT_queueGameOverCompositePanelChanges(
 RT_GameOverPanelUpload *state,const u8 panel[672],u16 *cursor,
 u16 cellBudget,u16 byteBudget);
/* Source entry ROW changes exactly23 cells starting two cells into its score
   row. A known source write range avoids rescanning the entire panel. Only
   [first,first+count) is validated/published; other cells remain unchanged.
   Set cursor=first for each revision; QUEUED means cursor=first+count. */
RT_CompositePanelResult RT_queueGameOverCompositePanelRange(
 RT_GameOverPanelUpload *state,const u8 panel[672],u16 first,u16 count,
 u16 *cursor,u16 cellBudget,u16 byteBudget);
#endif
