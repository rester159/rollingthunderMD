#include "rt_enemy_throw.h"
#include <assert.h>
#include <stdio.h>
static RT_TerrainMaterial material;
static unsigned hits;
u32 rtSpriteBirthEpoch;
const RT_TerrainMaterial *RT_motionProbe(const RT_WorldStage *s,const RT_MotionBody *b,const RT_WorldCamera *c,s16 x,s16 y)
{(void)s;(void)b;(void)c;assert(x==1 && y==-1);return &material;}
const RT_TerrainMaterial *RT_terrainWindowProbe(const RT_TerrainWindow *w,s32 x,s32 y,s16 cx,s16 cy)
{(void)w;(void)x;(void)y;assert(cx==1 && cy==-1);return &material;}
static u8 target(const RT_Projectile *s,void *c){(void)s;(void)c;hits++;return 0;}
RT_EnemyResult RT_throwMotionStep(RT_ProjectilePool *p,u8 slot,RT_ThrowMotion *m,
 const RT_WorldCamera *c,RT_ThrowProbe probe,RT_ThrowHit hit,void *context,RT_WeaponEvent *event)
{(void)p;(void)slot;(void)m;(void)c;(void)probe;(void)hit;(void)context;(void)event;assert(0);return RT_ENEMY_UNAVAILABLE;}
int main(void)
{
    RT_WorldStage stage={0};RT_WorldCamera camera={0};RT_TerrainWindow window={.ready=1};
    RT_Agent agents[2]={{0}};RT_AgentCombat combat[2]={{0}};RT_ThrowMotion motion[49]={{0}};
    unsigned cases=0;
    for(unsigned flags=0;flags<256;flags++)for(unsigned layer=0;layer<4;layer++)
    for(unsigned enemy=0;enemy<2;enemy++)for(unsigned left=0;left<2;left++)for(unsigned path=0;path<3;path++){
        for(unsigned i=0;i<4;i++)material.collision[i]=i==layer?flags:255;
        RT_ProjectilePool pool={0};pool.active=1;
        pool.slots[0]=(RT_Projectile){.x=1600,.y=1792,.velocityX=left?-64:64,.kind=RT_PROJECTILE_SHOT,
            .layer=layer*2,.flags=(left?RT_PROJECTILE_LEFT:0)|(enemy?RT_PROJECTILE_ENEMY:0)};hits=0;
        if(!path)RT_projectilesStep(&pool,agents,2,&stage,&camera,target,0);
        else if(path==1)assert(RT_projectilesStepPublished(&pool,agents,2,&stage,&camera,&window,target,0));
        else{
            RT_ProjectilePassContext c={.stage=&stage,.camera=&camera,.motion=motion,.bulletHit=target,
                .scene={.terrain=&window,.players={agents,combat,2}}};
            RT_ProjectilePassResult r;assert(RT_projectilesUpdateAll(&pool,&c,&r));
        }
        assert(pool.slots[0].kind==((flags&1)?RT_PROJECTILE_IMPACT:RT_PROJECTILE_SHOT));
        assert(hits==!(flags&1));assert(pool.active==1);
        cases++;
    }
    printf("%u projectile material cases passed\n",cases);
}
u8 RT_throwTerrainProbe(const RT_Projectile *o,const RT_ThrowMotion *m,u8 *a,u8 *b,void *c)
{(void)o;(void)m;(void)a;(void)b;(void)c;assert(0);return 0;}
u8 RT_throwSceneHit(const RT_Projectile *o,const RT_ThrowMotion *m,u8 b,void *c)
{(void)o;(void)m;(void)b;(void)c;assert(0);return 0;}
