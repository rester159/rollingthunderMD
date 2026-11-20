#ifndef RT_NATIVE_COMBAT_H
#define RT_NATIVE_COMBAT_H
#include "rt_enemies.h"
typedef struct {s16 yOffset;u16 height;s16 xOffset[2];u16 width;} RT_ContactBox;
extern const RT_ContactBox rtContactBoxes[128];
extern const RT_ContactBox rtBulletContactBox;
/* Per-player combat inputs and pending reactions. The scene owns life management
 * and reaction dispatch; collision never swaps a bound player. */
enum {RT_REACTION_NONE,RT_REACTION_SHOT_RIGHT,RT_REACTION_SHOT_LEFT,RT_REACTION_CLOSE_HURT,RT_REACTION_CLOSE_FATAL,RT_REACTION_THROW_HURT_RIGHT,RT_REACTION_THROW_HURT_LEFT};
typedef struct {u16 invincibleTicks;u8 hidden,damagePending,reaction,counterattack,priorityDamage,health,hitFlags,removed;} RT_AgentCombat;
/* A pending fatal reaction or its 105-update collapse is not yet a removed
 * actor, even if the independently draining health counter has reached zero. */
u8 RT_agentHasFatalAnimation(const RT_Agent *agent,const RT_AgentCombat *combat);
typedef struct {RT_Agent *agents;RT_AgentCombat *combat;u8 players;} RT_PlayerTargets;
/* Source962A normalization, hitbox perception and negative-pose eligibility.
 * Caller chooses the target player; no source player-record binding occurs. */
/* Snapshot of the selected player's source hitbox for one ordered actor pass. */
typedef struct {u16 x,y; s16 width,height;u8 attributes,flags;} RT_EnemyPerceptionTarget;
void RT_enemyPreparePerception(const RT_Agent *player,RT_EnemyPerceptionTarget *target);
/* Shared BD56 geometry only, before family-specific contact eligibility. */
u8 RT_enemyPerceiveGeometryPrepared(RT_Enemy *enemy,const RT_EnemyPerceptionTarget *target);
/* Original BCDA contact after geometry. command is native event02 on nonfatal
 * contact; caller owns delivery. Invincibility is the Home-mode extension. */
u8 RT_highEnemyPerceivePrepared(RT_Enemy *enemy,const RT_EnemyPerceptionTarget *target,
    RT_AgentCombat *combat,u8 *command);
u8 RT_enemyPerceivePrepared(RT_Enemy *enemy,const RT_EnemyPerceptionTarget *target);
/* CPU2 962A body contact for ordinary (low) families, after the shared
 * geometry above: decision bit 0x40 means the hitboxes overlap on the same
 * floor. Damages the selected player (record13 gate) or returns actor
 * selector 0x78 while that player is already reacting. Invincibility is the
 * Home-mode extension. Returns zero when the actor keeps its state. */
u8 RT_enemyBodyContact(RT_Enemy *enemy,const RT_EnemyPerceptionTarget *target,
    RT_AgentCombat *combat);
/* Ready entry requires non-null target and a live enemy. */
u8 RT_enemyPerceiveReady(RT_Enemy *enemy,const RT_Agent *player);
u8 RT_enemyPerceive(RT_Enemy *enemy,const RT_Agent *player);
typedef struct {RT_EnemyPool *enemies;u8 visibleActors;} RT_EnemyTargets;
/* Source pool-order collision queues61/62 and consumes one health unit;
 * the actor pass initializes that request on its following update. */
u8 RT_playerBulletHitEnemies(const RT_Projectile *shot,void *context);
extern const RT_EnemyEntry rtEnemyCloseEntries[4]; /* poses204..207 */
RT_EnemyResult RT_enemyCloseStep(RT_Enemy *enemy,RT_PlayerTargets *targets,u8 *selector);
/* Dispatches close attack/recoil/recovery and applies outgoing entries. */
RT_EnemyResult RT_enemyCloseUpdate(RT_Enemy *enemy,RT_PlayerTargets *targets,
    const RT_WorldCamera *camera,s16 accelerationY,RT_EnemyProbe probe,void *context,
    u8 sourceCounter,u8 workspaceFlags,u8 fallbackSelector);
typedef enum {RT_AGENT_HIT_WAIT,RT_AGENT_HIT_TERMINAL,RT_AGENT_HIT_INVALID} RT_AgentHitResult;
u8 RT_agentHitEnter(RT_Agent *agent,RT_AgentCombat *combat,u8 family,u8 direction,u8 *sound);
RT_AgentHitResult RT_agentHitStep(RT_Agent *agent,RT_AgentCombat *combat);
typedef enum {RT_AGENT_TERMINAL_INVALID,RT_AGENT_TERMINAL_RECOVER,RT_AGENT_TERMINAL_REMOVE,RT_AGENT_TERMINAL_SIGNAL} RT_AgentTerminalResult;
/* REMOVE asks the scene to reset its ten stage-event channels and decrement
 * live-player counts; SIGNAL asks it to advance the mode transition. */
RT_AgentTerminalResult RT_agentHitFinish(RT_Agent *agent,RT_AgentCombat *combat,
    const RT_WorldCamera *camera,u8 mode,u8 damageProtected,u8 *input,
    RT_AgentCommand *recovery);
typedef enum {RT_AGENT_CONTACT_WAIT,RT_AGENT_CONTACT_ADVANCED,RT_AGENT_CONTACT_INPUT,RT_AGENT_CONTACT_INVALID,RT_AGENT_CONTACT_FELL_OUT} RT_AgentContactResult;
u8 RT_agentContactEnter(RT_Agent *agent,RT_AgentCombat *combat);
/* Call after native player movement; collision is the player vertical result. */
RT_AgentContactResult RT_agentContactStep(RT_Agent *agent,RT_AgentCombat *combat,
    u8 collision,u8 *input);
RT_AgentContactResult RT_agentContactUpdate(RT_Agent *agent,RT_AgentCombat *combat,
    u8 input,const RT_WorldStage *stage,RT_WorldCamera *camera,u8 lockedCamera,
    RT_AnchorScene *scene);
u8 RT_agentConsumeReaction(RT_Agent *agent,RT_AgentCombat *combat,u8 *sound);
void RT_agentDamageTick(const RT_Agent *agent,const RT_WorldCamera *camera,
    RT_AgentCombat *combat,u8 damageProtected);
/* Returns actor selector78 on a player counterattack, otherwise zero. */
u8 RT_enemyCloseHitPlayers(const RT_Enemy *enemy,RT_PlayerTargets *targets);
u8 RT_projectileOverlapsAgent(const RT_Projectile *shot,const RT_Agent *agent);
/* For use only after terrain accepts an enemy bullet. Returns the first hit
 * in stable player order, and queues its source-derived damage/reaction. */
u8 RT_enemyBulletHitPlayers(const RT_Projectile *shot,void *context);
#endif
