# Rolling Thunder MD — v0.9 Beta

Time to move Rolling Thunder MD into beta. This update brings Home save slots, more Arcade settings, a cleaner HUD, and another round of fixes and optimization.

## Home saves and Continue

Home Campaign now has three save slots stored in cartridge SRAM. Continue on the title screen takes you back to the start of your saved area, keeping your character selection, solo or co-op setup, credits, scores and ammunition.

These are area checkpoints, so you can put a campaign down and return to it later. Survival stays a single-session challenge.

## A cleaner HUD

The colored bars at the top and bottom are gone. The HUD now sits over the playing field, closer to the arcade presentation, so you can see the scenery behind it.

Home also shows pistol and machine-gun ammunition separately. This fixes a confusing case where an AMMO door correctly replenished pistol rounds while the HUD was still showing the machine-gun count. I also fixed two co-op refill issues when both players used the same supply door.

## Arcade options

Arcade now has its own Options and DIP Switches menus, with controller mapping, sound test, and settings for lives, bonus lives, timer, difficulty, invulnerability and demo sound. Settings are saved in SRAM. The attract sequence is still there, too.

The distinction stays the same: Arcade follows the original game's rules and structure, while Home adds character selection, two-player Campaign, Survival and saves. Both campaigns include the first five areas and their harder second-story versions.

## Behind the scenes

The game still fits in **32 megabits — 4 MiB**. Getting there has involved sharing repeated graphics, packing data more efficiently and streaming compressed audio. Frequently used player and enemy images stay ready to use, and the renderer reuses more work while scrolling and drawing busy scenes.

The optimization work keeps the stages, characters, animation frames and audio intact. Campaign timing has improved in the measured workloads, though busy two-player scenes can still slow down. I'm continuing to test those cases.

I also cleaned up Leila's gameplay sprites by removing the lipstick across her animation frames. Her original hair colors and the existing selection portraits remain.

Two-player Campaign is still one of my favorite additions to this port, especially having Leila alongside Albatross. And the soundtrack remains a huge part of why I love this game.

Thanks for the feedback and hardware reports. If you find a problem in this beta, please include your console or emulator, cartridge, area, game mode and whether you were playing solo or co-op. That makes it much easier to reproduce.
