# Rolling Thunder MD v0.93 Beta

Source snapshot: November 19, 2026.
ROM: `dist/Rolling_Thunder_MD_v0.93_Beta.bin`.
Matching symbols and full build metadata: `dist/v0.93-beta/`.

Size: **4,194,304 bytes (4 MiB / 32 Mbit)**.
SHA-256: `5016396a3a4fa46aeae50fdbcc35b612d97de17bcbbf4fba0c02edde2a746498`.

[Devlog](DEVLOG_0_93.md) · Evidence: [stage feedback](V0_92_STAGE_FEEDBACK.md),
[presentation feedback](V0_92_PRESENTATION_FEEDBACK.md).

## Changes since v0.92 (itch.io reports on v0.92)

- **Body contact (rollingthunder4).** Ordinary enemies now apply the arcade's
  CPU2 `$962A` contact step after the shared overlap test: half a life bar
  (`0x20`), hurt or fatal reaction, and `0x78` knockback of the enemy while
  the player is already reacting. Covers the common update, door-emerging
  states `0x58/0x5c` and type10. Previously only the high families (apes,
  lava ghosts) touched the player. Co-op extends the rule to both players,
  at most one contact per actor update.
- **Survival invisible wall (rollingthunder4).** `RT_survivalCheckpointCapture`
  refused to capture while `RT_Agent.anchorIndex` (last door/ladder, never
  cleared) was set, so the checkpoint stalled below the retirement margin and
  the camera stopped at `cameraMaxX`. Death respawned with the index reset.
  The check is removed; the action/hidden/layer/floor checks still exclude a
  player in a door or on a ladder.
- **Sprite budget.** Crowded scenes exceeding the 58 non-HUD SAT links
  aborted with UNFINISHED GRAPHICS (`rtAppFault` 33/34), reproducible on v0.92
  with controller input only. Shots are reserved first; an over-budget actor
  is left out of that frame, and the order rotates so a crowd flickers.
  Diagnostic counter `rtSpriteBudgetDrops`.
- **Home restart points.** Paid Home Campaign continues use the arcade's
  mid-area restart point (CPU1 `$A592`, `$CF`) once reached, instead of the
  area entrance.
- **Game-over Maboo (malc1976).** Maboo's monitor scene runs ~200 frames
  (PAL-scaled) with sound byte `67` (bank 2 clip 7): Arcade after score entry,
  Home before the GAME OVER page.
- **Stage 1 windows and fans (U.S. PLATINUM).** Window cells use the stage's
  black tile at low priority (192 cells Story 1, 96 Story 2). Top HUD sprites
  narrow to inked columns, freeing per-line sprite pixels (288 → 192) so both
  fan pieces draw; fan colours remapped to existing stage-palette blues.
  Stage banks 0 and 5 repinned.
- **Z80 music start-up (ROBOT-RANDOM, rollingthunder4).** Hardening only, no
  definite cause found: the driver stops timers, clears flags and the test
  register and programs both periods before enabling them; a `HEART` byte
  (`$1F0B`) advances per 60 Hz tick. The 68000 watchdog (top of the main
  loop, every 16 frames) reloads the driver after ~0.8 s without a heartbeat,
  except while the 68000 itself holds PROTECT. Counter
  `rtGameMusicWatchdogReloads`.
- v0.93 BETA footer.

Investigated with no change (arcade matches): Area 5 ceiling lasers, Area 7
restart point and timer, and shots passing crouching, dying, emerging or
other-floor enemies.

## Validation

On this ROM in Genesis Plus GX, all pass: watchdog, music start timing, DMA
handoff, raster reservation; Survival progress 1P/2P; sprite budget; Home
restart (reached, not-reached, area7); presentation (area1, arcade, home);
body contact (1P, P2); board fixtures (platform, stairs, mutant,
mutant-inside, door); game over (5 modes); soft reset; cold save; attract;
Home features; solo ammo door; door animation (9 cases). Contact soaks across
all ten areas in 1P/2P and Survival 2P raised no fault. Reports:
[validation/v0.93](validation/v0.93).

Fixture changes for body contact: walking and crowd fixtures hold players
invincible; the platform fixture keeps the area empty from the vault onward.
Each still fails on the build it was written against.

These are emulator results. The audio hardening in particular is unverified
on a Model 3 or Analogue SG; no physical-hardware certification is claimed.
