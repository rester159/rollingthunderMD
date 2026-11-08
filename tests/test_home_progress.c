#include <stdint.h>
typedef int8_t s8;typedef int16_t s16;
#include "rt_home_progress.h"
#include "rt_save.h"
#include <assert.h>
#include <stdio.h>
RT_SessionConfig rtSession;
volatile u8 rtSaveAvailable=1;
static u8 disk[4][48],present[4];
static RT_WorldStage world;
void RT_saveExtrasLoad(void){}
bool RT_saveExtraRead(u8 slot,u8 p[48]){if(slot>3||!present[slot])return FALSE;memcpy(p,disk[slot],48);return TRUE;}
bool RT_saveExtraWrite(u8 slot,const u8 p[48]){assert(slot<4);memcpy(disk[slot],p,48);present[slot]=1;return TRUE;}
RT_CampaignInit RT_campaignSceneInitialize(RT_CampaignScene *s,const RT_SessionConfig *cfg,u8 story,u8 area,u8 checkpoint,u8 set,u16 credits)
{assert(!checkpoint&&!set);memset(s,0,sizeof *s);world.story=story;world.area=area;s->stage=&world;s->players.selection=*cfg;s->players.home.players=cfg->players+1;s->players.home.credits=credits;for(u8 i=0;i<s->players.home.players;i++)s->players.combat[i].health=64;return RT_CAMPAIGN_INIT_OK;}
#include "../src/home_progress.c"
int main(void)
{
    RT_CampaignScene scene={0},loaded;RT_CampaignScore score={.high=40000,.players={12345,54321},.bonusIndex={1,2}},restored={0};
    RT_CampaignUpdate update={0};world.story=1;world.area=2;scene.stage=&world;scene.players.home.players=2;scene.players.home.credits=5;
    rtSession=(RT_SessionConfig){.mode=RT_MODE_HOME,.players=1,.agent={1,0}};
    for(u8 i=0;i<2;i++){scene.players.combat[i].health=64;scene.players.agents[i].ammoPrimary=255+i;scene.players.agents[i].ammoAlternate=100+i;scene.players.agents[i].weaponMode=1;}
    for(u8 slot=0;slot<3;slot++){
        rtHomeSaveSlot=slot;scene.players.home.credits=5+slot;assert(RT_homeCheckpoint(&scene,&score));assert(RT_homeSlotArea(slot)==8);
    }
    for(u8 slot=0;slot<3;slot++){
        assert(RT_homeSelectSave(slot)&&rtHomeResume);assert(RT_homeRestore(&loaded,&restored));
        assert(loaded.stage->story==1&&loaded.stage->area==2&&loaded.players.home.credits==5+slot);
        assert(rtSession.agent[0]==1&&rtSession.players==1);
        for(u8 i=0;i<2;i++)assert(restored.players[i]==score.players[i]&&restored.bonusIndex[i]==score.bonusIndex[i]&&loaded.players.agents[i].ammoPrimary==255+i&&loaded.players.agents[i].ammoAlternate==100+i&&loaded.players.agents[i].weaponMode==1);
    }
    // Resuming cannot grant a free life to a dead co-op partner.
    scene.players.combat[1].health=0;scene.players.combat[1].removed=1;
    rtHomeSaveSlot=2;assert(RT_homeCheckpoint(&scene,&score));
    assert(RT_homeSelectSave(2)&&RT_homeRestore(&loaded,&restored));
    assert(loaded.players.combat[0].health==64&&!loaded.players.combat[0].removed);
    assert(!loaded.players.combat[1].health&&loaded.players.combat[1].removed);
    disk[2][6]=0;assert(!RT_homeSelectSave(2));
    scene.players.combat[1].health=64;scene.players.combat[1].removed=0;
    // Both previous profile versions retain DIP settings and slot selection.
    for(u8 version=1;version<=2;version++){
        rtHomeSaveSlot=2;rtArcadeDips=7;RT_homeProfileStore();disk[3][0]=version;
        memset(disk[3]+4,255,4); // Legacy optional data is ignored.
        RT_homeProgressLoad();assert(rtHomeSaveSlot==2&&rtArcadeDips==7&&RT_homeSlotArea(0)==8);
        RT_homeProfileStore();assert(disk[3][0]==2);
        for(u8 i=4;i<48;i++)assert(disk[3][i]==0);
    }
    disk[1][1]=10;assert(!RT_homeSelectSave(1));
    puts("Three Home checkpoints, characters, credits, scores, ammo, dead partner handling and both legacy profile versions passed.");
}
