#ifndef RT_NATIVE_CAMPAIGN_SPRITES_H
#define RT_NATIVE_CAMPAIGN_SPRITES_H
#include "rt_campaign_display.h"
#include "rt_sprite_arena.h"
/* Every pose and pattern is compiled for exactly this terrain palette.
 * Shared ROM patterns may be referenced by several banks; no pixel conversion
 * or source instruction decoding takes place during gameplay. */
typedef struct {
    const RT_DisplayStage *stage;
    const RT_SpritePose *agents[4];
    const RT_SpritePose (*doors)[3];
    const RT_SpritePose *fans,*stagePoses,*decorations,*bullets,*throws;
    const u32 *doorPatterns,*fanPatterns,*bulletPatterns;
    const RT_PackedSpritePose *(*enemy)(u8 kind,u8 color,u16 pose);
    const RT_PackedSpritePose *(*highEnemy)(u8 kind,u8 color,u16 pose);
    u16 agentPoseCount,doorTiles,fanTiles,bulletTiles;
    u16 bulletPoseCount,throwFirstPose,throwPoseCount;
} RT_CampaignGraphics;
extern const RT_CampaignGraphics rtCampaignOpeningGraphics;
typedef struct {
    RT_SpriteBatch batch;
    RT_SpriteSlot players[2];
    RT_SpriteArena arena;
    u16 doorTile,fanTile,bulletTile;
    u8 ready,fault,streamStages,hud; /* 1 missing player art,2 stage pose,3 enemy,4 projectile,5 capacity */
    const RT_CampaignGraphics *graphics;
} RT_CampaignSprites;
/* Compiled graphics currently belong to the opening atlas. The display bridge
 * validates stage identity. Call after terrain initialization, before gameplay. */
u8 RT_campaignSpritesInitialize(RT_CampaignSprites *sprites,const RT_CampaignDisplay *view);
/* Terrain and sprite bank must share the same stage descriptor, so a palette
 * change cannot silently retain the previous area's sprite colors. */
/* Resident generated terrain uses the dynamic object arena with an explicit
 * terrain reservation, without creating a fictitious terrain cache. */
u8 RT_campaignSpritesInitializeGenerated(RT_CampaignSprites *sprites,
    const RT_CampaignDisplay *view,const RT_CampaignGraphics *graphics,u16 terrainTiles);
u8 RT_campaignSpritesInitializeBank(RT_CampaignSprites *sprites,
    const RT_CampaignDisplay *view,const RT_CampaignGraphics *graphics);
u8 RT_campaignSpritesUseHud(RT_CampaignSprites *sprites,u8 players);
/* Once per presented frame only. No simulation mutations; respects chosen
 * identity/costume and protection flicker. Unsupported art fails explicitly.
 * Caller commits SAT/scroll/terrain together, then acknowledges display DMA. */
u8 RT_campaignSpritesQueue(RT_CampaignSprites *sprites,const RT_CampaignScene *scene,
    RT_CampaignDisplay *view);
u8 RT_campaignSpritesReproject(RT_CampaignSprites*,const RT_CampaignScene*,RT_CampaignDisplay*);
#endif
