#ifndef RT_HOME_MODE_H
#define RT_HOME_MODE_H
#include <genesis.h>
typedef enum {RT_MODE_ARCADE, RT_MODE_HOME} RT_GameMode;
typedef enum {RT_HOME_CAMPAIGN, RT_HOME_SURVIVAL} RT_HomeGame;
typedef enum {RT_HOME_SOLO, RT_HOME_TWO_PLAYERS} RT_HomePlayers;
typedef enum {RT_AGENT_ALBATROSS, RT_AGENT_LEILA} RT_AgentChoice;
typedef struct {
    /* Keep the session byte layout stable for existing capture tooling. */
    u8 mode, players, agent[2], reserved;
    u8 game, costume[2];
} RT_SessionConfig;
extern RT_SessionConfig rtSession;
/* Two bits per controller; default B fire, A/C jump. */
extern u8 rtControlLayout;
u16 RT_jumpButtons(u8 player);
u16 RT_fireButtons(u8 player);
void RT_resetSession(void);
bool RT_isCoop(void);
/* Fixed-point 8.8 multipliers: base 1.0, Home co-op 1.5. */
u16 RT_homePopulationScale(void);
u16 RT_homeAttackScale(void);
void RT_beginModeSelect(void);
void RT_beginHomeGameOver(void);
/* Keep unfinished gameplay from silently launching the original solo agent. */
void RT_showHomePending(void);
/* Edge-triggered two-controller input. Returns true after session selection. */
bool RT_stepModeSelect(u16 pad1, u16 pad2);
void RT_drawModeSelect(void);
#endif
