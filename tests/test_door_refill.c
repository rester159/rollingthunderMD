/* Drive production refill logic with overlapping visits to one shared door. */
#include "rt_climb.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Exit selection belongs to agent.c; this fixture stops at that boundary. */
u8 RT_agentSelectInScene(RT_Agent *a,RT_AgentCommand next,const RT_WorldStage *s,
    const RT_WorldCamera *c,RT_AnchorScene *scene)
{(void)s;(void)c;(void)scene;a->action=next.action;a->phase=0;return 1;}
static void visit(RT_Agent *a)
{memset(a,0,sizeof *a);a->action=RT_AGENT_DOOR_ENTER;a->phase=6;a->anchorIndex=1;}
static void step(RT_Agent *a,RT_AnchorScene *s)
{assert(RT_agentClimbStep(a,0,0,0,s));}
int main(void)
{
    const unsigned primary[]={0,20,30,50},alternate[]={0,40,80,100};
    for(unsigned alt=0;alt<2;alt++)for(unsigned level=1;level<=3;level++)
    for(unsigned players=1;players<=2;players++){
        unsigned duration=(alt?alternate:primary)[level];
        unsigned amount=duration*(players==2?3:2)/2;
        for(unsigned delay=0;delay<=duration+1;delay++){
            RT_Anchor door={0};door.flags=(level<<3)|(alt?32:0);
            RT_AnchorScene scene={&door,1,players};RT_Agent a,b;visit(&a);visit(&b);
            step(&a,&scene);
            for(unsigned i=0;i<delay;i++)if(a.action==RT_AGENT_DOOR_ENTER)step(&a,&scene);
            /* The second player reached row 6 just as the first consumed it. */
            unsigned available=!!(door.flags&24);step(&b,&scene);
            for(unsigned i=0;i<300;i++){
                if(a.action==RT_AGENT_DOOR_ENTER)step(&a,&scene);
                if(b.action==RT_AGENT_DOOR_ENTER)step(&b,&scene);
            }
            assert(a.action==RT_AGENT_DOOR_EXIT && b.action==RT_AGENT_DOOR_EXIT);
            assert(a.ammoPrimary==(alt?0:amount) && a.ammoAlternate==(alt?amount:0));
            assert(b.ammoPrimary==(!alt&&available?amount:0));
            assert(b.ammoAlternate==(alt&&available?amount:0));
        }
        RT_Anchor door={0};door.flags=(level<<3)|(alt?32:0);
        RT_AnchorScene scene={&door,1,players};RT_Agent a;visit(&a);
        a.ammoPrimary=a.ammoAlternate=9999;step(&a,&scene);
        for(unsigned i=0;i<duration;i++)step(&a,&scene);
        assert((alt?a.ammoAlternate:a.ammoPrimary)==amount-1);
        assert((alt?a.ammoPrimary:a.ammoAlternate)==9999);
    }
    puts("Door refills: all supply levels, both weapons, solo/co-op quantities, overlapping visits, consumed-door boundary and decimal wrap passed.");
}
