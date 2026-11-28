# Rolling Thunder MD — v0.94 Beta: a fix for the silent consoles

Thanks to ROBOT-RANDOM and malc1976 for the v0.93 sound reports. A Genesis Model 3 VA1 was silent in Arcade mode, and a Japanese NTSC MegaDrive was silent in Home mode. The detail that cracked it: playing Home, pressing reset, then playing Arcade gave sound, and the problem started in v0.91. Thank you for testing that carefully.

The download is **Rolling_Thunder_MD_v0.94_Beta.bin**, still **32 megabits — 4 MiB**. This is a sound-only update; gameplay is the same as v0.93.

## What was wrong

When the game copies graphics, it briefly pauses the sound chip so the two don't fight over the cartridge. The title screen uses the same pause on every frame for its colour effects.

If one of those title-screen interrupts arrived a little late, the game could lose track of a pause it had already asked for. From then on, every later pause thought someone else owned it, and nobody ever released it. The sound chip stayed paused for good: no music and no sound effects. Only a reset, which reloads the sound driver, cleared it.

Whether that interrupt arrives late depends on tiny timing differences between consoles. That's why it hit different machines in different modes, and why emulators never showed it. v0.91 is when the sound driver started loading at the title screen, which is when this became possible.

## What changed

- The game now keeps an exact count of who has paused the sound chip, and only the last one releases it.
- The title screen always releases its previous pause before asking for a new one.
- The sound watchdog added in v0.93 now notices a pause that nothing owns and releases it, so sound comes back by itself.

## Testing and hardware

I can't reproduce the exact console timing in an emulator, so the tests force the stuck state directly. v0.93 stays silent; v0.94 recovers immediately. The music, title-screen, attract, Home, reset, presentation and game-over tests all pass. These are emulator results, not real-hardware certification.

**If you had silence before, please try v0.94 from a cold power-on in both Home and Arcade** and tell me what you hear, with your console model and cartridge.
