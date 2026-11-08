#include "rt_type20.h"
#include "rt_high_animation.h"
#include "rt_high_enemy_motion.h"
#include <assert.h>
#include <stdio.h>
static u8 wallCode,entryCode,entryFlags;
static unsigned calls;
static u8 probe(s16 x,s16 y,u8 *code,u8 *flags,void *unused)
{
    (void)unused;calls++;
    if(x){assert(y==0);*code=wallCode;*flags=1;}
    else{assert(y==1 || y==-3);*code=entryCode;*flags=entryFlags;}
    return 1;
}
int main(void)
{
    RT_WorldCamera camera={0};unsigned cases=0;
    /* Every terrain code, both wall sides, both walking states, all four
     * variations and refused/accepted floor changes. Real sweep and entry. */
    for(unsigned code=0;code<256;code++)for(unsigned left=0;left<2;left++)
    for(unsigned alternate=0;alternate<2;alternate++)for(unsigned variant=0;variant<4;variant++)
    for(unsigned refuse=0;refuse<2;refuse++){
        RT_Enemy e={0};u8 sound=0;u16 award=0;
        e.state.kind=32;e.state.x=1536;e.state.y=1600;
        e.state.action=e.state.spawnAction=(alternate?0x40:4)|(left?2:1);
        e.variation=variant<<4;e.state.timer=17;e.state.row=3;
        e.velocityX=left?-20:20;wallCode=code;
        entryCode=refuse?0x23:0;entryFlags=refuse?0:0x10;calls=0;
        RT_Type20Motion c={.camera=&camera,.probe=probe,.sound=&sound,.award=&award};
        assert(RT_type20WalkStep(&e,&c)==RT_ENEMY_CONTINUE);
        u8 expected=code==0x19 || code==0x22?0x48:
            code==0x25 || code==0x27?(refuse?0x48:0x28):code==0x2d?(refuse?0x48:0x2c):0x24;
        assert(e.state.action==(expected|(left?2:1)));
        assert(e.state.spawnAction==e.state.action && e.state.row==0 && (e.byte12&1));
        assert(e.state.x==1536 && e.state.timer==rtHighStateRows[0][expected>>2].timer);
        assert(calls==((code==0x25 || code==0x27 || code==0x2d)?2:1));
        if(expected==0x24)assert(e.velocityX==(left?-48:48) && e.velocityY==-96 && sound==0x63);
        if(expected==0x28)assert(e.velocityY==-160);
        if(expected==0x2c)assert(e.velocityY==-64);
        cases++;
    }
    printf("%u real wall-sweep/response cases passed\n",cases);
}
