#ifndef RT_NATIVE_AGENT_H
#define RT_NATIVE_AGENT_H
#include "rt_motion.h"
#include "rt_anchors.h"
typedef enum {
    RT_AGENT_IDLE,RT_AGENT_WALK,RT_AGENT_LOOK,RT_AGENT_LOWER,RT_AGENT_KNEEL,
    RT_AGENT_RISE,RT_AGENT_FIRE,RT_AGENT_KNEEL_FIRE,RT_AGENT_JUMP,
    RT_AGENT_RUNNING_JUMP,RT_AGENT_VAULT,RT_AGENT_DROP,RT_AGENT_FALL,
    RT_AGENT_VAULT_FALL,RT_AGENT_STAIRS_UP,RT_AGENT_STAIRS_DOWN,
    RT_AGENT_AUTO_FIRE,RT_AGENT_AUTO_KNEEL_FIRE,
    RT_AGENT_DOOR_ENTER,RT_AGENT_DOOR_EXIT,RT_AGENT_CLIMB_UP,RT_AGENT_CLIMB_DOWN,
    RT_AGENT_HIT_60,RT_AGENT_HIT_64,RT_AGENT_HIT_68,RT_AGENT_HIT_6C,RT_AGENT_CONTACT,
    RT_AGENT_ACTION_COUNT
} RT_AgentAction;
typedef struct {u8 action,direction;} RT_AgentCommand; /* direction0 retains facing */
typedef struct {u8 duration,contactPose,pose,reserved;} RT_AgentFrame;
typedef struct {const RT_AgentFrame *frames;u8 count,reserved;} RT_AgentSequence;
typedef struct {
    RT_MotionBody body;
    u8 action,direction,phase,timer,pose,contactPose;
    u8 fireLatch,jumpLatch,fireRepeat,weaponMode;
    u8 sounds[2],soundCount,unsupported,stairCameraFlags;
    u8 doorSupply; /* Latched refill flags; occupies the former alignment byte. */
    u16 ammoPrimary,ammoAlternate; /* Native decimal counts, 0..9999. */
    u8 shotRequest,alternateReset;
    u8 anchorIndex,interactionEvent; /* index+1; zero means no linked anchor */
} RT_Agent;
extern const RT_AgentSequence rtAgentSequences[RT_AGENT_ACTION_COUNT];
extern const RT_AgentCommand rtAgentInputMaps[13][20];
/* Consume shotRequest after each step, before updating this agent again.
 * Bits encode pending, alternate, kneeling, selected left and prior left. */
enum {RT_SHOT_PENDING=1,RT_SHOT_ALTERNATE=2,RT_SHOT_KNEEL=4,
      RT_SHOT_LEFT=8,RT_SHOT_PRIOR_LEFT=16};
enum {RT_INTERACTION_NONE,RT_INTERACTION_VOICE,RT_INTERACTION_ALTERNATE};
/* Supported now: normal human idle/walk/posture/jump/vault/drop/fall/stairs/fire.
 * Requests needing interactive-object or death integration are reported
 * explicitly, never replaced with an invented idle/walk action. */
u8 RT_agentSelect(RT_Agent *agent,RT_AgentCommand command,const RT_WorldStage *stage,
    const RT_WorldCamera *camera);
u8 RT_agentStep(RT_Agent *agent,u8 input,const RT_WorldStage *stage,
    RT_WorldCamera *camera,u8 lockedCamera);
/* A scene shares anchor state between players. Step every player first, then
 * each door once. Keep linked anchor slots stable for the duration of a climb.
 * NULL scene is the same explicit no-interaction environment as the wrappers. */
u8 RT_agentSelectInScene(RT_Agent *agent,RT_AgentCommand command,
    const RT_WorldStage *stage,const RT_WorldCamera *camera,RT_AnchorScene *scene);
u8 RT_agentStepInScene(RT_Agent *agent,u8 input,const RT_WorldStage *stage,
    RT_WorldCamera *camera,u8 lockedCamera,RT_AnchorScene *scene);
/* Direction is already four-way filtered:0 none,1 right,2 left,3 up,4 down.
 * Pressed bits are edges supplied by the controller owner, independently per
 * player. A held jump does not auto-repeat after landing. */
u8 RT_agentInput(RT_Agent *agent,u8 direction,u8 jumpPressed,u8 firePressed,u8 fireHeld);
#endif
