# v0.92 itch feedback — game-over Maboo, Area 1 windows and fans

## Reports

- malc1976: after the last life is lost, the arcade shows Geldra leader Maboo
  laughing; the port did not.
- U.S. PLATINUM INTERACTIVE DEVELOPMENT (v0.92, PAL clone + Chinese EverDrive
  V3): Stage 1's background windows are cyan instead of black, and the ceiling
  fans show only their right half.

## Arcade evidence (owner-supplied revision 3 set, FBNeo, headless)

Game over, measured at the board's 60.6 Hz:

1. `PLAYER ONE / GAME OVER` on black (~230 refreshes, game-over music).
2. TOP 5 initials entry, only when the score qualifies (seeded score test).
3. The Geldra monitor with Maboo's laughing portrait, face visible about 200
   refreshes. Two refreshes after it appears the MCU receives 63701X byte
   `67` (bank 2, lane 1, clip 7); that clip's 15-refresh decaying bursts match
   the recorded "ha-ha-ha" envelope.
4. `TO CONTINUE GAME INSERT COIN` countdown, then attract.

Area 1 windows: both display planes hold the empty tile in the four upper-floor
window openings (Story 1 cells X104–111/116–123/200–207/212–219, rows 6–11;
Story 2 the first two), so the Genesis backdrop (the teal wall color) showed
through. The arcade shows black there.

Area 1 fans had two independent faults; the fan art itself is complete:

- **Sprite pixel overflow.** Fans hang at screen lines −9…22, the same lines as
  the top HUD. The HUD was nine fixed 32×24 sprites (x 16–304), using 288 of
  H40's 320 sprite pixels per line even over blank cells. The SAT dump shows the
  fan's right piece taking the last 32 pixels and the left piece dropped by
  the VDP — exactly "only the right half".
- **Colors.** The arcade draws fans with sprite color group 65 (PROM
  `rt1-4.5v`), a blue ramp up to (0,5,7). The palette fit put them on palette 1,
  whose nearest entries are teal (0,3,3)/(0,4,4); (0,3,3) is the Area 1 wall
  itself, so the lighter blades also blended into the wall.

## Changes

- `src/main.c`: new `APP_MABOO` state reuses the attract's Maboo portrait scene
  (completion selector 5, which alternates the laughing poses every eight
  services) for 200 refreshes (PAL-scaled) and posts sound byte `67`.
  Arcade: after the score-entry flow and before attract (not after the ending).
  Home Campaign/Survival: before the GAME OVER page. The sample driver is
  loaded before the scene takes VInt; the post itself only writes the mailbox.
  `rtGameOverMabooShown` counts visits for tests.
- `tools/fix_stage_windows.py`: puts the stage's existing opaque black tile
  (low priority, palette 0) on plane 1 in cells empty on both planes, Area 1
  maps only. 192 cells (Story 1) and 96 (Story 2). No tile or palette added.
- `src/campaign_hud_display.c`, `src/sprites.c`: each top HUD block sprite is
  narrowed to its inked columns and hidden when blank (recomputed when rows
  0–2 change; tiles are column-major so only the first tile moves). The
  default Area 1 HUD drops from 288 to 192 sprite pixels per line, leaving
  room for two complete fans. Glyph art and the SAT layout/links are unchanged.
- `tools/fix_fan_colors.py`: remaps the fan pens dark→light onto an existing
  blue ramp — black, (0,0,3), (0,0,4), (0,0,7) — palette 2 in Story 1 and
  palette 0 in Story 2, excluding the area's dominant wall colors. Pixels,
  geometry and CRAM are unchanged; the stage palettes have no light sky-blue,
  so the lightest blade is pure blue rather than the arcade's (0,5,7).
- `tools/repin_stage_banks.py`; stage banks 0 and 5 repinned in
  `assets/stages/manifest.json`.

## Validation

`tests/test_presentation_feedback.py` (`--case area1|arcade|home`). Screenshots
in `docs/validation/v0.93/before/` (v0.92) and `docs/validation/v0.93/after/`.
These are emulator results; physical hardware remains untested.

Candidate `dist/fix-presentation/RollingThunderMD.bin` (4 MiB, packed/refined,
standard cart), SHA-256
`f94dc7b2a9c98dcbb469824962c119d321bf65c3acfeaa73005fffcd44b10452`. The build's
full art verification passes with stage banks 0 and 5 repinned.

| Check (GX) | v0.92 | candidate |
| --- | --- | --- |
| Area 1 black window opening | fail (teal) | pass |
| Blue fan pixels, top 40 lines | 0 | 496 (both pieces drawn) |
| Arcade game over → score flow → Maboo → attract | no Maboo | pass |
| Home game over → Maboo → GAME OVER page | no Maboo | pass |
| Audio RMS during Maboo, 15-refresh windows | — | 4077, 2631, 2036 … decaying (laugh) |

Also passing on the candidate: `test_game_over_feedback.py` (all five cases),
`test_door_animation.py` (9 cases), `test_attract.py`, `test_hud_sprite_order.py`,
`test_recent_gameplay.py` (all five cases). A 2P score of 987,654 / 123,456
still renders completely (`after/hud-2p-scores.png`). `test_hud_backing.py` no
longer parses the current `hud??_data.c` format (fails before reaching the
ROMs; unrelated to this change).
