#include "rt_home_progress.h"
#include "rt_save.h"
u8 rtArcadeDips,rtHomeSaveSlot,rtHomeResume;
static u32 read32(const u8 *p){return (u32)p[0]<<24|(u32)p[1]<<16|(u16)p[2]<<8|p[3];}
static void write32(u8 *p,u32 n){p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n;}
static u16 read16(const u8 *p){return (u16)p[0]<<8|p[1];}
static void write16(u8 *p,u16 n){p[0]=n>>8;p[1]=n;}
static bool valid(const u8 *p)
{
    if(p[0]!=1||p[1]>9||p[2]>1||p[3]>3||p[24]>1||p[25]>1||p[26]>2||p[27]>2||p[28]>1||!p[6]||(p[6]&~((1<<(p[2]+1))-1)))return FALSE;
    for(u8 i=0;i<2;i++)if(read32(p+8+i*4)>999999||read16(p+16+i*4)>9999||read16(p+18+i*4)>9999)return FALSE;
    return TRUE;
}
void RT_homeProgressLoad(void)
{
    u8 p[48];RT_saveExtrasLoad();
    rtArcadeDips=rtHomeSaveSlot=0;
    if(RT_saveExtraRead(3,p)&&(p[0]==1||p[0]==2)){
        rtArcadeDips=p[1]&63;rtHomeSaveSlot=p[2]<3?p[2]:0;
    }
}
void RT_homeProfileStore(void)
{
    u8 p[48]={0};p[0]=2;p[1]=rtArcadeDips;p[2]=rtHomeSaveSlot;
    RT_saveExtraWrite(3,p);
}
u8 RT_homeSlotArea(u8 slot)
{u8 p[48];return RT_saveExtraRead(slot,p)&&valid(p)?p[1]+1:0;}
bool RT_homeSelectSave(u8 slot)
{
    u8 p[48];if(slot>2||!RT_saveExtraRead(slot,p)||!valid(p))return FALSE;
    rtHomeSaveSlot=slot;rtHomeResume=1;
    rtSession.mode=RT_MODE_HOME;rtSession.game=RT_HOME_CAMPAIGN;rtSession.players=p[2];
    rtSession.agent[0]=p[3]&1;rtSession.agent[1]=(p[3]>>1)&1;
    rtSession.costume[0]=rtSession.costume[1]=0;return TRUE;
}
bool RT_homeRestore(RT_CampaignScene *scene,RT_CampaignScore *score)
{
    u8 p[48];if(!RT_saveExtraRead(rtHomeSaveSlot,p)||!valid(p))return FALSE;
    if(RT_campaignSceneInitialize(scene,&rtSession,p[1]/5,p[1]%5,0,0,read16(p+4)))return FALSE;
    for(u8 i=0;i<2;i++){
        score->players[i]=read32(p+8+i*4);score->bonusIndex[i]=p[26+i];
        if(score->players[i]>score->high)score->high=score->players[i];
        RT_Agent *a=scene->players.agents+i;
        a->ammoPrimary=read16(p+16+i*4);a->ammoAlternate=read16(p+18+i*4);a->weaponMode=p[24+i];
        scene->players.home.spawn[i]=*a;
        if(!(p[6]&(1<<i))){scene->players.combat[i].health=0;scene->players.combat[i].removed=1;}
    }
    return TRUE;
}
bool RT_homeCheckpoint(const RT_CampaignScene *scene,const RT_CampaignScore *score)
{
    if(rtSession.mode!=RT_MODE_HOME||rtSession.game!=RT_HOME_CAMPAIGN)return TRUE;
    u8 p[48]={0};p[0]=1;p[1]=scene->stage->story*5+scene->stage->area;
    p[2]=rtSession.players;p[3]=rtSession.agent[0]|rtSession.agent[1]<<1;
    write16(p+4,scene->players.home.credits);
    for(u8 i=0;i<scene->players.home.players;i++)if(scene->players.combat[i].health&&!scene->players.combat[i].removed)p[6]|=1<<i;
    for(u8 i=0;i<2;i++){
        write32(p+8+i*4,score->players[i]);p[26+i]=score->bonusIndex[i];
        const RT_Agent *a=scene->players.agents+i;
        write16(p+16+i*4,a->ammoPrimary);write16(p+18+i*4,a->ammoAlternate);p[24+i]=a->weaponMode;
    }
    bool ok=RT_saveExtraWrite(rtHomeSaveSlot,p);RT_homeProfileStore();return ok;
}
