# Rolling Thunder MD — v0.92 Beta: stairs, bullets and a sound-driver tweak

This is a small maintenance update. The download is **Rolling_Thunder_MD_v0.92_Beta.bin**, still **32 megabits — 4 MiB**.

## Stage 2 bullets, rechecked

I went back to the report about crouching on the platform above the crates in Stage 2 and having your bullets vanish, plus the same thing at certain points on the stairs. I replayed both spots in the emulator on v0.9 and on the current build. On v0.9, crouched shots there stopped after a few pixels. Since v0.91 they fly the full width of the screen.

If you still see this, please check your title screen says v0.91 or later. Firing **up** a staircase can still stop a shot when it reaches the steps above you. The arcade does the same.

There's now an automated stairs test alongside the platform test, so this can't quietly come back.

## Sound driver

I changed how the main CPU waits for the sound chip during graphics transfers. The checks are now spaced about one sample apart instead of bunched together. That should leave the sound CPU freer while sampled sounds play. The music and sound data are unchanged.

## Testing

On the emulator, all the gameplay checks pass, along with the sound-driver handoff, music start timing, soft reset and audio soak tests. As always, these are emulator results, not real-hardware certification. Reports from real consoles are very welcome.

The open audio issues from v0.91 are still open: track 7 distortion, later skips in Main BGM 1, some instrument and percussion differences, and the faint high tone.

If you find another issue, please include the version, console or emulator, cartridge, stage, mode, and whether you're playing solo or co-op. Thanks for helping make the port better.
