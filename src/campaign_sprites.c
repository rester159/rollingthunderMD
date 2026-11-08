#define RT_BANKED_CAMPAIGN 1
#define RT_BANKED_CAMPAIGN 1
#include "rt_campaign_sprites.h"
#ifndef RT_BANKED_CAMPAIGN
#include "rt_agent_poses.h"
#include "rt_door_poses.h"
#include "rt_fan_poses.h"
#include "rt_stream_stage_poses.h"
#include "rt_static_stage_poses.h"
#include "rt_projectile_poses.h"
#include "rt_throw_poses.h"
#include "rt_enemy_atlas.h"
#include "rt_high_enemy_graphics.h"
const RT_CampaignGraphics rtCampaignOpeningGraphics={
    &rtDisplayOpening,{rtAgentPoses[0],rtAgentPoses[1],rtAgentPoses[2],rtAgentPoses[3]},
    rtDoorPoses,rtFanPoses,rtStreamStagePoses,rtStaticStagePoses,rtProjectilePoses,rtThrowPoses,
    rtDoorPatterns,rtFanPatterns,rtProjectilePatterns,RT_enemyGraphic,RT_highEnemyGraphic,
    RT_AGENT_POSE_COUNT,RT_DOOR_PATTERN_COUNT,RT_FAN_PATTERN_COUNT,RT_PROJECTILE_PATTERN_COUNT,
    20,RT_THROW_FIRST_POSE,RT_THROW_POSE_COUNT
};
u8 RT_campaignSpritesInitialize(RT_CampaignSprites *s,const RT_CampaignDisplay *view)
{
    return RT_campaignSpritesInitializeBank(s,view,&rtCampaignOpeningGraphics);
}
#endif
void RT_reprojectionReset(void);
static u8 initialize(RT_CampaignSprites *s,const RT_CampaignDisplay *view,
    const RT_CampaignGraphics *g,u16 dynamicTerrainTiles)
{
    RT_reprojectionReset();
    if(s){s->ready=0;s->graphics=0;}
    if(!s || !view || !view->display.ready || !view->display.stage ||
       !g || g->stage!=view->display.stage || !g->agentPoseCount ||
       !g->agents[0] || !g->agents[1] || !g->agents[2] || !g->agents[3] ||
       !g->doors || !g->fans || !g->stagePoses || !g->decorations ||
       !g->bullets || !g->throws || !g->doorPatterns || !g->fanPatterns ||
       !g->bulletPatterns || !g->enemy || !g->highEnemy || !g->bulletPoseCount ||
       !g->throwPoseCount || !g->bulletTiles)return 0;
    s->graphics=g;
    s->ready=0;s->fault=0;s->hud=0;s->streamStages=dynamicTerrainTiles!=0;
    s->doorTile=dynamicTerrainTiles?dynamicTerrainTiles:view->display.stage->patternCount;
    s->fanTile=s->doorTile+g->doorTiles;
    u16 playerTile=s->streamStages?s->doorTile:s->fanTile+g->fanTiles;
    s->bulletTile=playerTile+128;
    if(!RT_spriteSlotInitialize(s->players,playerTile,64) ||
       !RT_spriteSlotInitialize(s->players+1,playerTile+64,64) ||
       !RT_spriteArenaInitialize(&s->arena,s->bulletTile+g->bulletTiles,s->streamStages?45:34))return 0;
    if(!s->streamStages){
        VDP_loadTileData(g->doorPatterns,s->doorTile,g->doorTiles,CPU);
        VDP_loadTileData(g->fanPatterns,s->fanTile,g->fanTiles,CPU);
    }
    VDP_loadTileData(g->bulletPatterns,s->bulletTile,g->bulletTiles,CPU);
    s->ready=1;return 1;
}
u8 RT_campaignSpritesInitializeBank(RT_CampaignSprites *s,const RT_CampaignDisplay *view,
    const RT_CampaignGraphics *g)
{return initialize(s,view,g,view && view->stream?view->stream->tiles.slots:0);}
u8 RT_campaignSpritesInitializeGenerated(RT_CampaignSprites *s,const RT_CampaignDisplay *view,
    const RT_CampaignGraphics *g,u16 terrainTiles)
{
    if(!view || !view->display.stage || !terrainTiles || terrainTiles>640 ||
       view->display.stage->patternCount>terrainTiles)return 0;
    return initialize(s,view,g,terrainTiles);
}
u8 RT_campaignSpritesUseHud(RT_CampaignSprites *s,u8 players)
{
    if(!s || !s->ready || !s->streamStages || !s->graphics)return 0;
    if(players<1 || players>2)return 0;
    if(players==1){
        /* Solo sessions cannot join a second player until returning to the
         * menu. Give that unused 64-tile reservation to the exact-pose arena. */
        s->bulletTile-=64;
        VDP_loadTileData(s->graphics->bulletPatterns,s->bulletTile,s->graphics->bulletTiles,CPU);
    }
    u16 first=s->bulletTile+s->graphics->bulletTiles;
    if(first>=1536 || !RT_spriteArenaInitialize(&s->arena,first,min(RT_SPRITE_ARENA_BLOCKS,(1536-first)/16)))return 0;
    s->hud=1;return 1;
}
/* RT_campaignSpritesQueue lives with the held-group records it publishes:
 * see campaign_sprite_reprojection.c. */
