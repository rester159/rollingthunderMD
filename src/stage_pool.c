#include <genesis.h>
extern u32 rtSpriteBirthEpoch;
#include "rt_stage_pool.h"
/* Campaign anchors stay fixed between admission, retirement and rebasing.
 * Remember the camera interval before any anchor crosses a visibility edge. */
static RT_StagePool *visibilityPool;
static s16 visibilityLeft,visibilityRight,visibilityTop,visibilityBottom;
static u8 visibilityCount,visibilityPopulation;
void RT_stagePoolInvalidateVisibility(RT_StagePool *pool)
{if(visibilityPool==pool)visibilityPool=0;}
static void visibilityInterval(s32 coordinate,s32 lo,s32 hi,s32 *lower,s32 *upper)
{
    if(coordinate<lo){if(*upper>=lo)*upper=lo-1;}
    else if(coordinate>hi){if(*lower<=hi)*lower=hi+1;}
    else{if(*lower<lo)*lower=lo;if(*upper>hi)*upper=hi;}
}
u8 RT_stagePoolRefreshStaticVisibility(RT_StagePool *pool,const RT_WorldCamera *camera)
{
    if(!pool || !camera)return 255;
    if(visibilityPool==pool && visibilityPopulation==pool->count &&
       camera->x>=visibilityLeft && camera->x<=visibilityRight &&
       camera->y>=visibilityTop && camera->y<=visibilityBottom)return visibilityCount;
    u8 count=RT_stagePoolRefreshVisibility(pool,camera);
    if(count==255)return count;
    s32 left=-32768,right=32767,top=-32768,bottom=32767;
    for(u8 i=0;i<RT_NATIVE_STAGE_CAPACITY;i++){
        const RT_Anchor *a=pool->objects+i;
        if(a->kind==255)continue;
        /* X uses floor and Y uses ceil because their source edge tests have
         * opposite inclusivity. Shifts avoid overflow at s32 limits. */
        s32 x=a->x>>4,y=(a->y>>4)+!!((u32)a->y&15);
        visibilityInterval(camera->x,x-319,x+32,&left,&right);
        visibilityInterval(camera->y,y-(a->kind<8?208:272),
            y-(a->kind<8?33:17),&top,&bottom);
    }
    visibilityPool=pool;visibilityCount=count;visibilityPopulation=pool->count;
    visibilityLeft=left;visibilityRight=right;visibilityTop=top;visibilityBottom=bottom;
    return count;
}
u8 RT_stagePoolInitialize(RT_StagePool *pool,const RT_StageEventLevel *level)
{
    if(!pool || !level || !level->events || !level->cells || !level->columns || !level->rows)return 0;
    RT_stagePoolInvalidateVisibility(pool);
    pool->level=level;pool->count=0;
    for(u16 i=0;i<RT_NATIVE_STAGE_CAPACITY;i++){
        pool->objects[i]=(RT_Anchor){0};pool->objects[i].kind=255;
        pool->definitions[i]=65535;
    }
    return 1;
}
u8 RT_stagePoolEventVisitor(const RT_StageEvent *event,u16 index,void *context)
{
    RT_StagePool *pool=context;
    if(!pool || !pool->level || !event || index>=pool->level->eventCount || event->kind>31)return 0;
    for(u16 i=0;i<RT_NATIVE_STAGE_CAPACITY;i++)if(pool->objects[i].kind==255){
        RT_Anchor *object=pool->objects+i;
        RT_stagePoolInvalidateVisibility(pool);
    rtSpriteBirthEpoch++;
        object->x=(s32)event->x*16;object->y=(s32)event->y*16;
        object->kind=event->kind;object->flags=event->initialState;
        object->layer=event->parameter>>5;
        object->phase=object->timer=object->spawnDelay=object->pose=0;
        /* Source constructors mark new records hidden; visibility belongs
         * to the subsequent service pass, not allocation. */
        object->visible=0;pool->definitions[i]=index;return 1;
    }
    return 0;
}
u8 RT_stagePoolSpawn(RT_StagePool *pool,const RT_EventWindowRequest *request)
{
    if(!pool || !pool->level || !request)return 0;
    return RT_stageVisitEvents(pool->level,request->gridX,request->gridY,
        (RT_EventWindowEdge)request->edge,&pool->count,RT_stagePoolEventVisitor,pool);
}
u8 RT_stagePoolRelease(RT_StagePool *pool,u8 slot)
{
    if(!pool || slot>=RT_NATIVE_STAGE_CAPACITY || !pool->count ||
       pool->objects[slot].kind==255)return 0;
    RT_stagePoolInvalidateVisibility(pool);
    pool->objects[slot].kind=255;pool->count--;return 1;
}

u8 RT_stagePoolCull(RT_StagePool *pool,const RT_WorldCamera *camera,u8 boundaries)
{
    if(!pool || !camera || boundaries>15)return 255;
    if(!pool->count || !boundaries || boundaries==3 || boundaries==7 || boundaries>=11)return 0;
    u8 remaining=pool->count,removed=0;
    s32 left=(s32)camera->x*16-0x700,right=(s32)camera->x*16+0x1900;
    s32 top=((s32)camera->y+256)*16-0x1800;
    s32 bottom=((s32)camera->y+256)*16+0x800;
    for(u16 i=0;i<RT_NATIVE_STAGE_CAPACITY;i++){
        RT_Anchor *object=pool->objects+i;
        if(object->kind==255)continue;
        if(!object->visible &&
           (((boundaries&RT_STAGE_BOUNDARY_RIGHT) && object->x<left) ||
            ((boundaries&RT_STAGE_BOUNDARY_LEFT) && object->x>=right) ||
            ((boundaries&RT_STAGE_BOUNDARY_UP) && object->y>bottom) ||
            ((boundaries&RT_STAGE_BOUNDARY_DOWN) && object->y<=top))){
            RT_stagePoolInvalidateVisibility(pool);
            object->kind=255;pool->count--;removed++;
        }
        if(!--remaining)return removed;
    }
    return 255;
}

u8 RT_stagePoolRefreshVisibility(RT_StagePool *pool,const RT_WorldCamera *camera)
{
    if(!pool || !camera)return 255;
    RT_stagePoolInvalidateVisibility(pool);
    u8 count=0,remaining=pool->count;
    s32 left=(s32)camera->x*16-0x200;
    s32 origin=((s32)camera->y+256)*16;
    /* Kinds below 8 and the others use two fixed vertical bands. */
    s32 topLow=origin-0xe00,topHigh=origin-0xf00;
    RT_Anchor *object=pool->objects,*end=pool->objects+RT_NATIVE_STAGE_CAPACITY;
    for(;remaining && object<end;object++){
        if(object->kind==255)continue;
        remaining--;
        s32 x=object->x,y=object->y;
        u8 visible=0;
        if((u32)x-(u32)left<0x1600UL){
            if(object->kind<8)visible=(u32)y-(u32)topLow-1<0xb00UL;
            else visible=(u32)y-(u32)topHigh-1<0x1000UL;
        }
        object->visible=visible;count+=visible;
    }
    return remaining?255:count;
}
u8 RT_stageFanStep(RT_Anchor *object)
{
    if(!object || object->kind!=8 || !object->visible)return 0;
    if(object->spawnDelay)object->spawnDelay--;
    object->timer++;
    if(!(object->timer&7)){
        object->phase=(object->phase+1)&3;
        object->pose=object->phase;
    }
    return 1;
}
u8 RT_stagePoolAdvanceWindow(RT_StagePool *pool,RT_EventWindow *window,
    const RT_WorldStage *stage,const RT_WorldCamera *previous,
    const RT_WorldCamera *current)
{
    if(!pool || !pool->level)return 0;
    RT_EventWindowRequest requests[2];
    u8 count=RT_eventWindowMove(window,stage,previous,current,requests),flags=0;
    if(count==255)return 0;
    for(u16 i=0;i<count;i++)flags|=1u<<(requests[i].edge-1);
    if(RT_stagePoolCull(pool,current,flags)==255)return 0;
    for(u16 i=0;i<count;i++)if(!RT_stagePoolSpawn(pool,requests+i))return 0;
    return 1;
}
