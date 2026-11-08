#ifndef RT_HIGH_VISIBILITY_H
#define RT_HIGH_VISIBILITY_H
#include "rt_enemies.h"
/* Exact early-return cases of all four high-family prepare services.
 * The caller validates the family/definition and records action coverage. */
static inline u8 RT_highEnemyDormant(RT_Enemy *e,u16 cameraX,u16 cameraY)
{
    s16 x=(s16)((u16)e->state.x-cameraX);
    s16 y=(s16)(cameraY-(u16)e->state.y);
    u8 outsideY=(u16)(y+0x400)>=0x1400;
    if((e->state.action&0xfc)==0x80){
        if(e->state.hidden && (outsideY || (e->state.spawnAction&2?x<0x1500:x>=-0x300)))return 1;
    }else if(outsideY || x>0x1500 || x< -0x300){e->state.hidden=1;return 1;}
    return 0;
}
#endif
