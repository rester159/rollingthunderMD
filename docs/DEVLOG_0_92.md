# Rolling Thunder MD — v0.92 Beta: doors, bullets, stairs and a few stubborn mutants

Thanks for all the reports on v0.9, especially from people testing on real hardware. This update fixes several gameplay and presentation problems you helped track down. The download is **Rolling_Thunder_MD_v0.92_Beta.bin**, and it still fits in **32 megabits — 4 MiB**.

## Gameplay fixes

- **Bullets near platforms and stairs:** in Stage 2, crouching and firing on the platform above the crates could make your bullets disappear almost immediately. The same thing happened at certain points on the stairs. Some floor and stair markers were being treated as solid walls. Shots now use the arcade's wall-collision rule, so crouched shots fly across the screen. Firing *up* a staircase can still stop a shot when it reaches the steps above you, as in the arcade. There are now automated tests for both spots, so this can't quietly come back.
- **Black door interiors:** opening a door could reveal a cyan-green rectangle. The cleared interiors now render black across all ten areas, while the textured cave entrances keep their artwork.
- **Yellow mutants in Stage 3:** these enemies could freeze against a wall or remain trapped in a column after spawning. Their missing wall response is restored, so they can turn, hop or change floors as intended.

I also rechecked the Stage 1 BULLETS door before the basement stairs. It supplies 50 pistol rounds, including when you're carrying the machine gun. Home's separate ammunition counters make that refill easier to see.

## Menus and presentation

- Home saves now show **SLOT 1, SLOT 2 and SLOT 3**, instead of repeating SLOT 3.
- The title-screen characters animate more smoothly. The idle menu was repeatedly checking save data and wasting time; it now saves settings when they change or when you start playing.
- Leave the Arcade menu idle for ten seconds and the attract sequence starts automatically.
- Palette transfers have been corrected to address stray colored pixels around the title and stage transitions.
- The yellow flash when opening the sound test has been fixed.
- The version footer now reads **v0.92 BETA**.

## Music and sound

I fixed a timing problem that could rush the first notes of a track while the sound driver caught up with its initialization work. The opening now keeps its intended rhythm.

I also changed how the main CPU waits for the sound chip during graphics transfers. Its checks are now spaced about one sample apart instead of bunched together, which leaves the sound CPU freer while sampled sounds play.

There is still audio work to do. The reported distortion, later skips in Main BGM 1, some instrument and percussion differences, and the faint high tone are still under investigation. This update does **not** claim to fix all of those. I love this soundtrack, and I want to get it right.

## Testing and hardware

The fixes pass the emulator regression, sound-driver and timing checks. All ten areas, both characters, two-player Campaign, Survival and Home SRAM saves remain included. These are emulator results, not real-hardware certification.

Thanks also to cgamebit for confirming that **v0.9** works on their Genesis Model 1 VA7 with an EverDrive V2 OS3.6 clone, as well as their V3 Pro. The new palette and sound-driver changes still need checking on real consoles.

If you find another issue, please include the version, console or emulator, cartridge, stage, mode, and whether you're playing solo or co-op. A short clip helps, too. Thanks for helping make the port better.
