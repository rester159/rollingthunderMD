#ifndef RT_NATIVE_CAMPAIGN_HUD_DISPLAY_H
#define RT_NATIVE_CAMPAIGN_HUD_DISPLAY_H
#include "rt_campaign_hud_art.h"
typedef struct {
    const RT_CampaignHudArt *art;
    RT_CampaignHud previous;
    u16 firstTile;
    u8 ready;
} RT_CampaignHudDisplay;
/* Transparent sprite overlay; both scrolling planes keep their stage art.
 * Begin requires display disabled and an empty DMA queue. Patterns occupy
 * D000..DFFF and F800..F9FF; SAT entries 0..3 and 62..79 belong to the HUD.
 * The legacy firstTile argument is retained for the display owner's API. */
u8 RT_campaignHudDisplayBegin(RT_CampaignHudDisplay *display,
    const RT_CampaignHudArt *art,u16 firstTile);
/* Queue only changed glyphs; DMA copies temporary pattern buffers. */
u8 RT_campaignHudDisplayQueue(RT_CampaignHudDisplay *display,const RT_CampaignHud *hud);
/* Bit per logical row. Caller accumulates dirty rows across skipped draws and
 * clears them only after a successful queue. */
u8 RT_campaignHudDisplayQueueRows(RT_CampaignHudDisplay *display,const RT_CampaignHud *hud,u8 dirtyRows);
/* End before menus or stage remapping; releases the shared title scratch. */
void RT_campaignHudDisplayEnd(RT_CampaignHudDisplay *display);
#endif
