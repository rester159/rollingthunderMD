#ifndef RT_NATIVE_CAMPAIGN_HUD_ART_H
#define RT_NATIVE_CAMPAIGN_HUD_ART_H
#include "rt_campaign_hud.h"
/* Atlas is compiled for one immutable stage palette. Attributes are relative
 * pattern IDs; the display owner adds its reserved VRAM firstTile. 65535 marks
 * unsupported glyph/style pairs. No color conversion occurs during gameplay. */
typedef struct {
    const u32 *patterns;
    const u16 (*attributes)[256];
    const RT_HudCell *initial;
    const u8 *groups;
    u16 patternCount;
    u8 groupCount,story,area;
    const u8 *inks;
} RT_CampaignHudArt;
#endif
