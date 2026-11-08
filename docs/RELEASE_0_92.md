# Rolling Thunder MD v0.92 Beta

Source snapshot: November 7, 2026.
ROM: `dist/Rolling_Thunder_MD_v0.92_Beta.bin`.
Matching symbols and full build metadata: `dist/v0.92-beta/`.

Size: **4,194,304 bytes (4 MiB / 32 Mbit)**.
SHA-256: `b50e548c41972be76daab4278bdafb6a71d77d990b7c0be7334a47cd7320805c`.

[Devlog](DEVLOG_0_92.md)

## Changes since v0.91

- Z80 DMA guard: while waiting for the ROM music driver to acknowledge
  PROTECT, the 68000 now spaces its bus checks about one sample period apart
  (160 checks × 35-loop gap) instead of 200 closely spaced checks. The total
  wait is no shorter than about 80% of the previous budget. The nested raster
  reservation from v0.91 is retained. A diagnostic counter,
  `rtZ80ProtectFallbacks`, records any guard that times out and halts the Z80.
- Stage 2 crate balcony and stair-tower bullets rechecked against the
  original report. The bug reproduces on v0.9 and is absent since v0.91.
  A new `--case stairs` gameplay fixture walks the first tower flight and
  fails on v0.9.
- v0.92 BETA footer.

Gameplay, graphics, stage content and music data are otherwise unchanged from
v0.91.

## Validation

Built with the packed/refined standard profile; all ten stage banks reproduce
and the four finite PCM clips pass placement checks. On this ROM, in Genesis
Plus GX:

- Gameplay fixtures: crate balcony, stairs, yellow mutant (wall and inside
  column) and black door interior pass. See [validation/v0.92](validation/v0.92).
- Arcade byte contracts and native projectile/mutant host cases pass.
- Z80 DMA release race, raster-window reservation, Main BGM 1 start rhythm,
  soft reset and the banked-scene audio soak pass.
- `rtZ80ProtectFallbacks` stayed at 0 across scripted play in three stages.

These are emulator results. No physical-hardware certification or full human
playthrough is claimed. Audio items listed as open in the v0.91 notes remain
open.
