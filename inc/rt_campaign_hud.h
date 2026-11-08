#ifndef RT_NATIVE_CAMPAIGN_HUD_H
#define RT_NATIVE_CAMPAIGN_HUD_H
#include "rt_campaign_score.h"
/* Logical text cells: the arcade's three top rows and single bottom row.
 * These are glyph/style IDs for an offline atlas, never emulated addresses. */
typedef struct {u8 glyph,group;} RT_HudCell;
typedef struct {RT_HudCell cells[144];} RT_CampaignHud;
/* Native decimal inputs; scores are units of10 points. Preserve labels and
 * cells the original HUD service does not write. Invalid inputs are inert. */
u8 RT_campaignHudScores(RT_CampaignHud *hud,const RT_CampaignScore *score,
    u8 lives0,u8 lives1);
u8 RT_campaignHudTimer(RT_CampaignHud *hud,u16 remaining);
u8 RT_campaignHudAmmo(RT_CampaignHud *hud,u16 remaining);
u8 RT_campaignHudHealth(RT_CampaignHud *hud,u8 health);
typedef struct {
    RT_CampaignHud hud;
    u32 high,score[2];
    u16 timer,ammo[2],credits,machineAmmo[2];
    u8 health[2],reserves,home,players,valid,dirtyRows,machineMask;
} RT_CampaignHudState;
u8 RT_campaignHudBegin(RT_CampaignHudState *state,const RT_HudCell initial[144],u8 home,u8 players);
/* Call after EVERY logical update, including skipped presentations: health
 * bars retain source partial writes as damage ticks count down. */
u8 RT_campaignHudUpdate(RT_CampaignHudState *state,const RT_CampaignScore *score,
    u16 timer,const u16 ammo[2],const u8 health[2],u16 credits,u8 reserves,
    const u16 machineAmmo[2],u8 machineMask);
#endif
