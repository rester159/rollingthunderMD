#ifndef RT_NATIVE_SPRITES_H
#define RT_NATIVE_SPRITES_H
#include "rt_display.h"

/* Immutable, build-time resolved geometry. Offsets are relative to an actor's
 * world-space foot anchor (pixels, y down). Facing variants retain the source's
 * independently authored X offsets; they need not be perfect mirrors. */
typedef struct {
    s16 x[2],y;
    s16 clipX[2],clipY;
    u16 tile,attribute[2];
    u8 size,priority,clipRule,reserved;
} RT_SpritePiece;
typedef struct {
    const u32 *patterns;
    const RT_SpritePiece *pieces;
    u16 tileCount;
    u8 pieceCount,reserved;
} RT_SpritePose;
/* Offline compiled rule0 components, contiguous right then left facing.
 * Biased clip intervals, device offsets and tile attributes are ready to add.
 * This format is only for original low-actor clipping; other rules use pieces. */
typedef struct {u16 clipX,clipY,x,y,attribute;u8 size,priority;} RT_PackedSpritePiece;
typedef struct {const RT_SpritePose *pose;const RT_PackedSpritePiece *pieces;} RT_PackedSpritePose;
/* These types already have the Genesis SAT's byte layout. */
typedef struct __attribute__((may_alias)) {u16 y;u8 size,link;u16 attribute,x;} RT_SpriteCommand;
typedef struct {
    RT_SpriteCommand storage[80];
    RT_SpriteCommand *commands;
    u16 count,queuedBytes;
    u8 failed,reserved;
    u8 orderKeys[80];
    /* Maximal runs of equal order keys, in submission order. Emitters link
     * each record to its predecessor, so HUD queues only patch run starts. */
    u8 runCount;
    u8 runStarts[80],runKeys[80];
} RT_SpriteBatch;
static inline void RT_spritesMarkRun(RT_SpriteBatch *batch,u16 start,u8 key)
{
    if(batch->count==start)return;
    if(batch->runCount && batch->runKeys[batch->runCount-1]==key)return;
    batch->runStarts[batch->runCount]=start;batch->runKeys[batch->runCount++]=key;
}
/* Drop records from start onward, with any runs they began. */
static inline void RT_spritesTruncate(RT_SpriteBatch *batch,u16 start)
{
    batch->count=start;
    while(batch->runCount && batch->runStarts[batch->runCount-1]>=start)batch->runCount--;
}
typedef struct {
    const RT_SpritePose *pose;
    u16 firstTile,capacity;
} RT_SpriteSlot;
typedef struct {
    const RT_SpritePose *pose;
    s16 x,y;
    u8 facingLeft,priority,visible,poolRank;
} RT_SpriteInstance;
enum {
    RT_SPRITE_POOL_STAGE=0, RT_SPRITE_POOL_ACTOR=1,
    RT_SPRITE_POOL_PROJECTILE=2, RT_SPRITE_POOL_RELATED=3
};

/* Each slot exclusively owns a fixed VRAM range below C000. Initialize slots
 * after terrain initialization; never share overlapping ranges. Pose pixels
 * are CPU-mapped ROM or tagged physical addresses. Use DMA_QUEUE for tagged
 * poses; its backend temporarily maps window 7, then restores that mapping. */
u8 RT_spriteSlotInitialize(RT_SpriteSlot *slot,u16 firstTile,u16 capacity);
void RT_spritesBegin(RT_SpriteBatch *batch);
/* Reserve 22 SAT entries for the transparent HUD; native actor records
 * retain their four-entry offset and link after the persistent HUD tail. */
void RT_spritesBeginHud(RT_SpriteBatch *batch);
u8 RT_spriteSubmit(RT_SpriteBatch *batch,RT_SpriteSlot *slot,
    const RT_SpriteInstance *instance,const RT_WorldCamera *camera);
/* Atlas must already be resident at firstTile. Emits SAT only; no uploads or
 * per-instance VRAM reservation. Pose piece tiles index that complete atlas. */
u8 RT_spriteSubmitResident(RT_SpriteBatch *batch,u16 firstTile,
    const RT_SpriteInstance *instance,const RT_WorldCamera *camera);
/* Submit records in ascending slot order within each pool, with each pose's
 * pieces in source component order. Queue once after all submissions, then
 * flush with the terrain in VBlank. Descending instance priority/pool rank and
 * reversed ties reproduce the source's later-entry-wins overlap on Genesis.
 * Piece priority affects the foreground plane test, not the record sort key.
 * Queue may exchange command zero with the sorted head; follow SAT links to
 * read the resulting display order. Begin a new batch before submitting again. */
u8 RT_spritesQueue(RT_SpriteBatch *batch);
u8 RT_spriteSubmitPacked(RT_SpriteBatch *batch,RT_SpriteSlot *slot,
    const RT_SpriteInstance *instance,const RT_WorldCamera *camera,
    const RT_PackedSpritePiece *pieces);
/* Only compiled rule3 stage poses with identical facing geometry/attributes.
 * The complete pose atlas must already be resident. No DMA is submitted. */
u8 RT_spriteSubmitStage(RT_SpriteBatch *batch,u16 firstTile,
    const RT_SpriteInstance *instance,const RT_WorldCamera *camera);
/* Same rule3 geometry, with pose uploads owned by a streamed arena slot. */
u8 RT_spriteSubmitStageCached(RT_SpriteBatch *batch,RT_SpriteSlot *slot,
    const RT_SpriteInstance *instance,const RT_WorldCamera *camera);
/* Main-thread preparation permits one published frame plus one prepared frame.
 * Drain the published DMA queue before publishing the prepared frame and
 * beginning another preparation. Deferred HUD output exclusively owns the SDK
 * SAT cache: no SDK sprite calls or video reset until that queue is drained.
 * Synchronous queues retain snapshot-copy semantics. Discard requires sprite
 * residency reinitialization before gameplay resumes. */
/* Upload pose patterns into slot (prepared when deferred); no SAT output. */
/* Geometry can differ while its complete pixel stream is identical. Only
 * storage residency is shared; each instance still emits its own pieces. */
static inline u8 RT_spritePixelsMatch(const RT_SpritePose *a,const RT_SpritePose *b)
{return a==b || (a && b && a->patterns==b->patterns && a->tileCount==b->tileCount);}
u8 RT_spriteUpload(RT_SpriteBatch *batch,RT_SpriteSlot *slot,const RT_SpritePose *pose);
u8 RT_spritesDeferred(void);
void RT_spritesDefer(void);
u8 RT_spritesPublishPrepared(void);
void RT_spritesDiscardPrepared(void);
#endif
