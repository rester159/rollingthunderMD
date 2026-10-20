# PAL hardware feedback fixes — unreleased, October 18, 2026

The user reported successful v0.81 gameplay on a PAL clone with a Chinese
EverDrive V3, with intermittent Area 2 AMMO-door trouble and cyan/green HUD
background bars. The user later clarified solo Home, with several visits failing to refill.
See [the solo follow-up](SOLO_AMMO_FEEDBACK.md); the co-op races below do not
explain that report.
The hardware report is recorded in [hardware compatibility](HARDWARE_COMPATIBILITY.md).

## Local changes

The HUD atlas change in v0.81 replaced opaque black pixels with transparency.
That exposed the stage backdrop and terrain through the top and bottom bars.
Restoring the original opaque backing fixes this in PAL and NTSC without changing
stage palettes, glyph colors, tile counts, or the upload/rendering path.

Two shared-door refill races were reproduced and fixed:

- When one player consumes a primary-ammo door, its shared flag changes to the
  consumed state. A second player already refilling previously interpreted that
  flag as alternate ammo. Each player now remembers the supply type when their
  refill starts, in a formerly unused alignment byte; the 42-byte agent layout
  and total RAM allocation are unchanged.
- A player can advance past the supply check just before the other consumes the
  door. Starting the resulting zero-duration refill previously wrapped its timer
  and produced a long, incorrect refill. That player now exits the empty door.

These races can affect Area 2's primary AMMO door, but are not confirmation that
we reproduced the user's unspecified hardware symptom. Solo/Arcade traversal and
any entry/animation-specific symptom still need the user's details or a retest.

## Validation

Evidence: [validation files](validation/pal-feedback/).

- All ten stages compared in PAL and NTSC: world pixels identical; HUD changes
  only replace exposed background with black. Linked atlas words preserve every
  non-backing pixel. Stage 1/2 PAL before/after images are included.
- Native cartridge Area 2 refill fixtures fail on the preceding candidate and
  pass on the fixed build in PAL and NTSC. The overlap fixture previously gave
  P2 28 primary rounds and 2 alternate rounds; it now gives 30 primary rounds.
  The consumed-door boundary exits in one refresh instead of remaining stuck
  past the fixture's 180-refresh limit. These use RAM setup at refill boundaries,
  not a playthrough to the authored door.
- Host refill tests cover all three supply quantities, both weapon types,
  solo/co-op quantities, staggered overlap, consumption boundaries and decimal
  wrap. Normal quantities and the co-op 50% bonus are retained.
- Frontend tests pass in Genesis Plus GX and PicoDrive. Regional boot/pause/reset
  checks, nine arcade-referenced door-animation cases and both agents' door
  visibility tests pass.
- Four matched Campaign samples (Areas 1/2, 1P/2P, 900 updates each) have identical
  refresh and presentation counts before/after: 901/899, 902/875, 900/898 and
  914/781 respectively. These samples do not establish whole-game or physical
  hardware performance.

## Artifact

Local ROM: `dist/pal-feedback-candidate/RollingThunderMD.bin`.
Matching symbols and build metadata are in that folder.

- ROM: 4,194,304 bytes, 4 MiB / 32 Mbit.
- Payload end: 4,189,406 bytes; 4,898 bytes spare.
- SHA-256: `5dd1fa706f26f993eeaf24edf3b2d81595a1bd404c03e353e1186afc25b8bb56`.
- Includes the existing lossless optimization candidate and reverted/original
  accepted portraits.

No release, push, or itch.io upload was made. `CURRENT_BUILD.txt` and the released
`dist/Rolling_Thunder_MD_v0.81_Alpha.bin` remain unchanged. ROM files remain ignored.
