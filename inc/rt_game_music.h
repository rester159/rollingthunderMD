#ifndef RT_GAME_MUSIC_H
#define RT_GAME_MUSIC_H
#include <genesis.h>
/* Level music on the Z80 music driver (sequencer + YM2151->YM2612/PSG
 * translation run there). Start/Stop from the main thread. */
u8 RT_gameMusicTrackFor(u8 story,u8 area);
/* Install without playing, while the title display/raster is disabled. */
void RT_gameMusicPrepare(void);
void RT_gameMusicStart(u8 track);
void RT_gameMusicStop(void);
u8 RT_gameMusicActive(void);
/* No-op: the Z80 clocks itself. Kept for menu callers. */
void RT_gameMusicPoll(void);
/* Sound-test diagnostic: silence the three PSG-routed parts. */
void RT_gameMusicSetPsgMuted(u8 muted);
u8 RT_gameMusicPsgMuted(void);
/* Active and the sequence has not reached its terminal end. */
u8 RT_gameMusicPlaying(void);
/* Arcade 63701X sound bytes (bit5 lane, bit6 bank 0/2, bits0-4 clip), in
 * source order; zero bytes are skipped. Played by the Z80 driver's DAC lanes. */
void RT_gameSoundPost(const u8 *bytes,u8 count);
#endif
