#include "rt_arcade_session.h"
u8 RT_arcadeSessionInitialize(RT_ArcadeSession *s,const RT_CampaignScene *scene,u8 setting)
{
    if(!s || !scene || !scene->ready || !scene->stage || setting>1 ||
       scene->players.selection.mode!=RT_MODE_ARCADE)return 0;
    s->reserves=setting?4:2;s->checkpoint=0;
    s->story=scene->stage->story;s->area=scene->stage->area;s->previousWindow=scene->objectWindow;return 1;
}
u8 RT_restartPointBegin(RT_ArcadeSession *s,const RT_CampaignScene *scene)
{
    if(!s || !scene || !scene->ready || !scene->stage)return 0;
    s->checkpoint=0;s->story=scene->stage->story;s->area=scene->stage->area;
    s->previousWindow=scene->objectWindow;return 1;
}
u8 RT_restartPointFor(const RT_ArcadeSession *s,u8 story,u8 area)
{
    return s && s->checkpoint && s->story==story && s->area==area;
}
void RT_arcadeObserveWindow(RT_ArcadeSession *s,const RT_CampaignScene *scene)
{
    if(!s || !scene || !scene->ready || !scene->stage || scene->stage->story!=s->story || scene->stage->area!=s->area)return;
    const RT_EventWindow *now=&scene->objectWindow;
    if(!s->checkpoint && (now->gridX!=s->previousWindow.gridX || now->gridY!=s->previousWindow.gridY)){
        RT_EventWindow checkpoint;
        if(RT_eventWindowInitialize(&checkpoint,scene->stage,1) &&
           now->gridX==checkpoint.gridX && now->gridY==checkpoint.gridY)s->checkpoint=1;
    }
    s->previousWindow=*now;
}
u8 RT_arcadeClaimRestart(RT_ArcadeSession *s)
{
    if(!s || !s->reserves)return 0;
    s->reserves--;return 1;
}
