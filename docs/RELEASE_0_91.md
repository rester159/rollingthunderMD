# Rolling Thunder MD v0.91 Beta

Source snapshot: October 20, 2026.
ROM: `dist/Rolling_Thunder_MD_v0.91_Beta.bin`.
Matching symbols and full build metadata: `dist/v0.91-beta/`.

Size: **4,194,304 bytes (4 MiB / 32 Mbit)**.
SHA-256: `15bbd465776b3b875896ebafd06166bdc3f9fa161977658a931949d2d7515675`.

[Devlog](DEVLOG_0_91.md) · [Detailed feedback review](V0_91_OCT20_FEEDBACK.md)

## Changes since v0.9

- Opaque black cleared door interiors across all ten areas; textured cave
  entrances and existing stage/sprite palettes preserved.
- Arcade wall-bit filtering in all three bullet terrain paths prevents shots
  hitting invisible platform and stair markers.
- Restored yellow-mutant wall actions and refused-floor-change fallback,
  including escape from the reported Stage 3 column spawn.
- Distinct Home save-slot labels and faster idle title animation.
- Automatic attract entry after ten idle seconds in the Arcade submenu.
- Bounded palette transfers and audio bus ownership correct stray palette
  pixels around title/intermission scenes; sound-test loading flash corrected.
- Music startup drops initialization tick backlog without discarding sampled
  audio, preserving the first notes' rhythm.
- v0.91 BETA footer.

## Validation and remaining work

The release is byte-identical to the tested full-build candidate. All ten stage
banks reproduce; packed sprite, map and audio checks pass. Reports in
[validation/v0.91](validation/v0.91) retain tested hashes and historical
candidate results. Current checks include 16 feature regression groups,
22 timing/menu/pacing checks, 12,288 projectile cases, 8,192 mutant wall cases,
four gameplay fixtures that fail on v0.9 and pass here, and the reported Stage 1
BULLETS door. PAL/NTSC and Genesis Plus GX/PicoDrive coverage is documented in
the individual reports. No full human playthrough or physical-hardware
certification is claimed.

The 900-update solo Stage 2 workload remains 952 refreshes/900 presentations.
Co-op records 929 refreshes/742 presentations versus v0.9's 928/741; corrected
bullet behavior changes the workload. These measurements are not a universal
frame-rate guarantee. No content was removed to retain the 32 Mbit size.

Track 7 headroom, later Main BGM 1 stalls, instrument/percussion fidelity and
the faint high tone remain open. See [audio findings](V0_91_AUDIO_FEEDBACK.md).
The palette changes still require real-console retesting.

ROMs, arcade archives, Sega chant and Namcot logo assets remain excluded from Git.
