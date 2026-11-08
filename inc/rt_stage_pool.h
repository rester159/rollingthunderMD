#ifndef RT_NATIVE_STAGE_POOL_H
#define RT_NATIVE_STAGE_POOL_H
#include "rt_stage_events.h"
#include "rt_anchors.h"
#define RT_NATIVE_STAGE_CAPACITY 48
typedef struct {
    const RT_StageEventLevel *level;
    RT_Anchor objects[RT_NATIVE_STAGE_CAPACITY];
    u16 definitions[RT_NATIVE_STAGE_CAPACITY];
    u8 count;
} RT_StagePool;
/* All kinds share stable slots. Definition IDs retain immutable constructor
 * metadata; no duplicate world positions or source records are needed.
 * Kinds4..31 still need their behavior/pose implementations. They must not
 * be treated as fully implemented merely because allocation is supported. */
u8 RT_stagePoolInitialize(RT_StagePool *pool,const RT_StageEventLevel *level);
u8 RT_stagePoolSpawn(RT_StagePool *pool,const RT_EventWindowRequest *request);
/* Constructor callback shared by authored and relocated event visitors.
 * Context is an initialized RT_StagePool. The visitor owns count increments;
 * this callback creates one slot and preserves the original definition index. */
u8 RT_stagePoolEventVisitor(const RT_StageEvent *event,u16 index,void *context);
/* Explicit release only. Caller owns any behavior-specific link side effects.
 * This function does not choose a scrolling removal policy. */
u8 RT_stagePoolRelease(RT_StagePool *pool,u8 slot);
enum {RT_STAGE_BOUNDARY_RIGHT=1,RT_STAGE_BOUNDARY_LEFT=2,
      RT_STAGE_BOUNDARY_UP=4,RT_STAGE_BOUNDARY_DOWN=8};
/* D385-equivalent removal after visibility refresh and before admitting new
 * strips. Only hidden objects can retire. Combined axis flags are evaluated
 * together. Links/other retained fields are untouched, as in the source.
 * Returns removal count, or255 on invalid arguments/population accounting. */
u8 RT_stagePoolCull(RT_StagePool *pool,const RT_WorldCamera *camera,u8 boundaries);
/* Visibility only: high-object behavior/timers belong to their own service.
 * Refresh, run object services, then advance the window. */
/* Cached form for fixed Campaign anchors. Invalidate before external writes
 * to coordinates, kinds or visibility; pool admission/retirement does so. */
void RT_stagePoolInvalidateVisibility(RT_StagePool *pool);
u8 RT_stagePoolRefreshStaticVisibility(RT_StagePool *pool,const RT_WorldCamera *camera);
u8 RT_stagePoolRefreshVisibility(RT_StagePool *pool,const RT_WorldCamera *camera);
/* Kind8 (CPU1 D8D6): visible fans advance every eight services. pose is
 * zero-based for fans, unlike doors; hidden fans retain their clock.
 * Returns 1 when serviced, 0 for hidden/non-fan/null objects. */
u8 RT_stageFanStep(RT_Anchor *object);
/* Call after all current object services and before the next update. Derive
 * combined boundary flags, retire hidden objects, then admit ordered strips.
 * Request processing is synchronous; failure requires caller error handling. */
u8 RT_stagePoolAdvanceWindow(RT_StagePool *pool,RT_EventWindow *window,
    const RT_WorldStage *stage,const RT_WorldCamera *previous,
    const RT_WorldCamera *current);
#endif
