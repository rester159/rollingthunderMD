#ifndef RT_HOME_PROGRESS_H
#define RT_HOME_PROGRESS_H
#include "rt_campaign_loop.h"
/* Cabinet options are independent of Home rules. Defaults match rev. 3. */
enum {RT_DIP_LIVES=1,RT_DIP_BONUS=2,RT_DIP_TIMER=4,RT_DIP_EASY=8,RT_DIP_INVINCIBLE=16,RT_DIP_DEMO_MUTE=32};
extern u8 rtArcadeDips,rtHomeSaveSlot,rtHomeResume;
void RT_homeProgressLoad(void);
void RT_homeProfileStore(void);
u8 RT_homeSlotArea(u8 slot);
bool RT_homeSelectSave(u8 slot);
bool RT_homeRestore(RT_CampaignScene *scene,RT_CampaignScore *score);
bool RT_homeCheckpoint(const RT_CampaignScene *scene,const RT_CampaignScore *score);
#endif
