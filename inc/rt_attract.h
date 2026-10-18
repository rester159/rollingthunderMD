#ifndef RT_ATTRACT_H
#define RT_ATTRACT_H
#include "rt_campaign_scene.h"
enum {RT_ATTRACT_OFF,RT_ATTRACT_TITLE,RT_ATTRACT_DEMO,RT_ATTRACT_SCORES,RT_ATTRACT_MABOO};
extern volatile u8 rtArcadeAttract,rtAttractScriptIndex;
extern volatile u32 rtAttractLoops,rtAttractDemoUpdates;
void RT_attractDemoInitialize(RT_CampaignScene *scene);
RT_HomeAgentStep RT_attractAgentStep(RT_Agent *agent,RT_AgentCombat *combat,
 const RT_WorldStage *stage,RT_WorldCamera *camera,RT_AnchorScene *anchors,u8 *sound);
#endif
