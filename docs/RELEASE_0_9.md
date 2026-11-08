# Rolling Thunder MD v0.9 Beta

Source snapshot: October 19, 2026. Local ROM:
`dist/Rolling_Thunder_MD_v0.9_Beta.bin`.
Matching symbols and complete build metadata: `dist/v0.9-beta/`.

Size: **4,194,304 bytes (4 MiB / 32 Mbit)**.
SHA-256: `ded14ecffd08d6b3cb97240a04bfbd80e75a3aebe1e7ffec58ed80eabbabca1c`.

[Dev update](DEVLOG_0_9.md) · [Home and Arcade details](HOME_FEATURES.md)

## Changes since v0.81

- Three cartridge SRAM Home Campaign slots and title-screen Continue. Resume at
  the saved area's entrance with characters, solo/co-op setup, credits, scores,
  ammunition and bonus-life progress. Survival is not resumable.
- Transparent HUD over the playing field, removing the opaque colored bands.
  Pistol and machine-gun ammunition display separately in Home.
- Fixed shared-door supply-type and empty-refill races in co-op. The separate
  ammunition counters also fix a reproduced solo case where a successful pistol
  refill was hidden behind the equipped machine-gun count.
- Arcade Options and DIP Switches menus: independent controller layouts, sound
  test, lives, bonus-life thresholds, timer, difficulty, invulnerability and demo
  sound. Settings persist in SRAM. Existing attract mode is retained.
- Main/Home/Arcade menu text uses gray/white lettering with a dark shadow.
  Initial selection footer identifies v0.9 BETA and the port credit.
- Lossless storage and audio-streaming improvements, graphic lookup reuse,
  common-pose preloading and faster background column handling. No stages,
  characters, animation frames or recorded samples were removed to fit 32 Mbit.
- Leila's gameplay lipstick removed in both costumes across all ten stage banks;
  original hair colors, existing portraits and enemy palettes retained.
- No built-in achievement system or RetroAchievements integration.

Both campaign modes retain the ten-area progression: five base stages followed
by their harder second-story counterparts, then the ending.

## Validation and limits

The normal build reproduces all ten frozen stage banks and checks sprite pixels,
geometry, background maps and recorded audio samples. Current-ROM emulator reports
cover three-slot cold SRAM restoration and controls on Genesis Plus GX and
PicoDrive, Arcade switches, PAL/NTSC solo ammo-door scenarios, and movement,
shooting and jumping in solo Campaign, co-op Campaign and co-op Survival.
Attract completed two full loops, accepted Start/Back in every phase and left
saves and records unchanged. The title footer was visually checked.
Reports are in `releases/0.9/`; earlier detailed feature and timing evidence is in
`validation/`, with the tested ROM hash retained in each report.

Campaign optimization measurements show improvements in tested workloads, not a
universal speedup or a guarantee of 60 FPS. Busy co-op scenes can still slow down.
The user reported v0.81 working on a PAL clone and Chinese EverDrive V3; v0.9 has
not been physically tested on that setup or certified across Model 1/2/3 consoles.
No full-game human playthrough is claimed by the automated checks.

ROMs, arcade archives, Sega chant and Namcot logo assets remain excluded from Git.
The Git source/tag update does not upload a ROM or publish an itch.io devlog.
