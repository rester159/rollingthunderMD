# Rolling Thunder MD — v0.93 Beta: enemies that push back, Maboo laughs, and a silent-console fix

Thanks for the reports on v0.92. This update works through everything you posted since it went up. The download is **Rolling_Thunder_MD_v0.93_Beta.bin**, and it still fits in **32 megabits — 4 MiB**.

## Gameplay fixes

- **You can no longer walk through enemies.** As in the arcade, touching a Masker or another ordinary enemy now costs half your life bar and knocks the enemy back. Before, only the yellow apes and lava ghosts did this. In two-player games it applies to both players.
- **Survival's invisible wall is gone.** After you used a door or a ladder, Survival stopped saving new checkpoints, so the level stopped loading ahead and you hit a wall. Dying cleared the problem, which is why getting killed was the only way through. Checkpoints now keep up with you.
- **No more "UNFINISHED GRAPHICS" crash in big crowds.** When a lot of enemies piled up on screen, the Genesis ran out of sprites and the game stopped with an error screen. Now the extra enemies flicker for a moment instead, and your shots always get drawn first.
- **Home continues now restart mid-area.** Once you pass an area's restart point, a continue puts you back there instead of at the start of the area. These are the arcade's own restart points.

## Presentation

- **Maboo laughs at you again.** At game over, Maboo now appears on the Geldra monitor and laughs, like the arcade. In Arcade it comes after high-score entry; in Home it comes before the GAME OVER menu.
- **Stage 1 windows are black** instead of cyan, in both versions of the area.
- **The ceiling fans are drawn in full and in blue.** The top HUD was using up the sprite space on those lines, so the left half of each fan was missing. I also fixed the fan colours, which were blending into the wall.

## Sound

There were reports of **no sound at all on a Genesis Model 3 VA1**, and **sound only on some power-ons on an Analogue SG**. I haven't found a definite cause, but I've made two changes:

- The sound driver no longer depends on what state the sound chip happens to be in at power-on.
- If the sound driver ever stops running, the game now notices within about a second and restarts it.

**If you had silence before, please try v0.93 and tell me what happens**, including your console model and cartridge.

## Things I checked that already match the arcade

- **Area 5 ceiling lasers:** I went through the arcade's Area 5 data and played it through in an arcade emulator. I found guards that drop from the ceiling, but no lasers. If you're thinking of a specific spot, a screenshot or clip would really help.
- **Area 7 checkpoints and timer:** the arcade has no mid-area restart point in Area 7, and its time limit is the same for every area. The port already matches both.
- **Shots that do nothing:** every case I found was arcade behaviour. Crouching gunmen duck under standing shots, enemies can't be hit while dying or coming out of a door, and enemies on another floor are out of reach. If you remember where it happened, please tell me the area and spot.

## Testing and hardware

All of this was tested in an emulator: new tests for each fix, the existing gameplay, door, game-over and save tests, and long automated runs through every area in one- and two-player play. These are emulator results, not real-hardware certification.

If you find another issue, please include the version, console or emulator, cartridge, stage, mode, and whether you're playing solo or co-op. A short clip helps, too. Thanks for helping make the port better.
