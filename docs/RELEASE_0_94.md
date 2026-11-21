# Rolling Thunder MD v0.94 Beta

Source snapshot: November 21, 2026.
ROM: `dist/Rolling_Thunder_MD_v0.94_Beta.bin`.
Matching symbols and full build metadata: `dist/v0.94-beta/`.

Size: **4,194,304 bytes (4 MiB / 32 Mbit)**.
SHA-256: `8fab3b0f4b8e1186f0f6cbbaec32659b2ac739996a9c8088a99df1d68916c4df`.

[Devlog](DEVLOG_0_94.md)

## Changes since v0.93 (itch.io reports on v0.93)

- **Permanent Z80 park (ROBOT-RANDOM: Model 3 VA1 silent in Arcade;
  malc1976: Japanese NTSC MegaDrive silent in Home).** `RT_z80DmaGuardBegin`
  inferred nesting from the Z80 PROTECT flag (`$1F30`): a guard that found it
  set returned token 3, which never clears it. The title raster carries its
  footer reservation across VBlank into the next frame; when the first HInt was
  late (line 99+), the body-prepare branch overwrote that token 2 without
  ending it. The flag then stayed high, every later guard became "nested", and
  the driver parked in RAM for good (no music, no SFX) until a driver reload
  cleared Z80 RAM, e.g. after a soft reset. The v0.93 watchdog skipped any
  held PROTECT, so it never recovered.
  - Ownership is now a 68000-side count (`protectOwners`); the last owner
    lowers the flag. Token 3 is gone; every driver load resets the count.
  - Both raster prepare branches end any carried token before beginning.
  - The watchdog judges a park by 68000 owners and lowers an unowned flag
    (`RT_z80ClearStaleProtect`, counter `rtZ80StaleProtectClears`).
- v0.94 BETA footer.

## Validation

Emulator (Genesis Plus GX) on this ROM: music watchdog (new stale-park case:
v0.93 stays parked, v0.94 recovers at once with no reload), raster reservation
(outer owner now injected through `protectOwners`), DMA handoff, music start
timing, transition CRAM timing and feedback, attract, Home features, soft reset
(arcade, campaign_1p, survival_2p), presentation (area1, arcade, home), game
over (arcade, campaign_1p). Reports: [validation/v0.94](validation/v0.94).

The interrupt-timing race itself was not reproduced in emulation; the stuck
state is injected. No physical-hardware certification is claimed.
