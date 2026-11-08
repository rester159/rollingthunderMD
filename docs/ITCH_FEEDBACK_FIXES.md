# Itch feedback test build — 2026-10-18

Local candidate: `dist/feedback-fixes-candidate/RollingThunderMD.bin`.
Size: 4,194,304 bytes (4 MiB / 32 Mbit), linear cartridge without SSF mapping.
SHA-256: `54eba2215c07860b6644426bf82d97fcbea3070dbe893e0edfab3fbedca9156d`.

Feedback source: https://rester159.itch.io/rolling-thunder-md/comments
Portrait work was excluded from this pass. Nothing was uploaded, and the published
v0.8 ROM and CURRENT_BUILD.txt were not replaced.

## Fixes

- Hide either player while fully inside a door, using the existing door action and
  layer state. Apply the same rule during held rendering frames.
- Restore bonus sound commands and transition music start/stop handling.
- Prevent a transient title palette raster fault during sound-driver loading from
  permanently freezing the interactive menu. Such faults are acknowledged and
  counted; ordinary cutscenes retain their strict fault handling. Synchronize
  title redraw and sound starts to VBlank. This is recovery, not a claim that a
  transient palette miss is impossible.
- Keep PAL DMA staging allocation at 7,200 bytes, matching NTSC. The prior 8,192
  byte allocation exhausted the boot heap during default-font loading. Transfer
  limits and allocation capacity checks remain in place.

The candidate also includes existing unreleased compression, controller mapping,
C-jump, HUD, audio handshake, and Arcade attract work. Those changes were tested
here, rather than recreated. The reported qualifying-score game-over failure was
reproduced in the published ROM; the candidate passes that path.

## Validation on the exact delivered ROM

All tests ran headlessly, without opening RetroArch or changing desktop focus.
Reports and matching symbols are beside the ROM.

- Genesis Plus GX and PicoDrive: all 13 sound-test tracks started three times
  each; sustained menu animation, return to Settings, reset and gameplay restart.
- Both cores: terminal-life handling in Campaign 1P/2P, Survival 1P/2P, and Arcade.
  Arcade includes qualifying-score entry and return through the attract flow.
- Both cores: frontend navigation, independent saved controls, C jump, selection
  rendering and settings persistence.
- GX: both agents hidden inside doors and visible when emerging; nine authored
  door animation cases across Arcade, Home 1P and Home 2P.
- GX: all nine ordinary transition selectors, audio output and return to gameplay.
- Nine regional startup cases across US, Europe and Japan, including dirty RAM/I/O
  launches on GX, plus pause/resume and reset/restart.
- Linear/no-mapper startup, Z80 stale-acknowledgement race regression, two complete
  Arcade attract loops, and scene/audio completion and restoration for ten stages.
- Valid ROM checksum and 4 MiB size; relevant runtime source hashes recorded in
  `feedback-source-hashes.json`.

Summary reports: `feedback-test-summary.json` (19 passing jobs),
`remaining-transitions.json` (6), `extra-checks.json` (5), and
`compatibility.json` (9 passing regional cases).

## Limits

KFusion, Mednafen and physical EverDrives were not directly tested. The linear
4 MiB layout removes the previous size/mapping dependency but does not certify
those platforms. Game-over tests seed terminal state, and transition tests select
the production scenes diagnostically; these are not full-game playthroughs.
Audio tests establish commands and audible output, not exact arcade waveforms.
Arcade attract is verified; full original start-game intro parity is not certified.
This pass did not establish a new gameplay FPS measurement or a 60 FPS guarantee.

Build isolation: `.local/feedback-work` was used because another task was building
in the shared workspace. Matching ROM, symbols and reports were copied together;
stale reports from earlier iterations were excluded.
