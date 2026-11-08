# 15 MiB older-cartridge candidate — October 17, 2026

**A separate test build exists; it has not replaced the accepted build.**
It targets the published Mega EverDrive v2/X7 extended-SSF protocol. It is not
yet release-qualified, and it does not solve compatibility with 7 MiB cartridges.

## Files

Generated under `dist/compatibility-candidate/`:

- `RollingThunderMD-MegaEverDriveV2-X7.bin`: 15,728,640 bytes. SHA-256:
  `109b7c5e2c51b34207e84d535ddb5dfc4145682ab6e16259ab4764e4bbe5d808`.
- `RollingThunderMD.bin`: standard-SSF2 comparison build, also 15 MiB. SHA-256:
  `7f531333fb5e3f75698dd8548b36b50a4c878b1982cc0c3cb2b4493ae10e9fbe`.
- Matching symbols, build metadata and `SHA256SUMS` accompany both files.

Both accepted 15.5 MiB ROMs in the original checkout retain their original hashes.
These candidates and source changes live in the isolated compatibility worktree.

## What changed

Identical geometry, 16 duplicate pose tables and four sets of identical public
graphics arrays are now shared across stages. The shared metadata occupies
228,800 bytes and is directly readable through two stable CPU memory windows.
The existing pixel-transfer window remains available.

The large raw pixel pool is byte-for-byte identical and its position within each
512 KiB bank is preserved. No graphics decompression or new graphics buffer is
used. All four recorded music clips are byte-for-byte identical in the assembled
ROM. No stages, modes, characters, graphics, animation rules or audio were removed.

The ROM ends before the v2/X7 OS reservation. Its extended-SSF save path uses the
documented upper portion of bank 31, with the existing protected write gate and
two-slot save format. This follows
[Krikzz's published protocol](https://krikzz.com/pub/support/mega-everdrive/x3x5x7/dev/extended_ssf-v2.txt);
it is not a physical hardware test.

## Checks completed

- Build-time comparison: 33,720 pose records, 29,790 packed-geometry records and
  808,960 background-map cells match the frozen reference; world data also match.
- Every sample of the four finite music recordings matches the accepted build.
- All ten stage images and ten high-score images match the accepted build in
  both the standard core and extended-SSF software model. Long high-score playback
  completes and restores its ROM mapping.
- Nine standard-profile region/boot probes pass across PicoDrive and Genesis
  Plus GX, including dirty starts.
- The v2/X7 software model passes six reset scenarios in each of Arcade,
  Campaign 1P/2P and Survival 1P/2P: 30 total. These include interrupted-save
  reset conditions. No prohibited OS-bank use or unsafe ROM writes was recorded.
- Three fresh emulator processes pass save restoration and corrupt-slot fallback.

## Measured performance

Each row below is a deterministic 1,800-refresh opening sample, about 30 seconds.
The figures count displayed frames, including presentations reusing artwork.
They include the workload's continue/game-over behavior and are not a claim of
sustained active-play FPS throughout a complete campaign.

| Comparison | Mode | Accepted displayed frames | Candidate | Simulation updates |
| --- | --- | ---: | ---: | --- |
| Extended-SSF software model | Campaign 1P | 1,736 | 1,736 | Match |
| Extended-SSF software model | Campaign 2P | 1,737 | 1,737 | Match |
| Extended-SSF software model | Survival 2P | 1,396 | 1,395 | Match |
| Standard core | Campaign 1P | 1,735 | 1,736 | Candidate +1 |
| Standard core | Campaign 2P | 1,737 | 1,737 | Match |
| Standard core | Survival 2P | 1,403 | 1,401 | Match |

The measured Survival differences are small (about 0.03 FPS in the cartridge
model and 0.07 FPS in the standard comparison). On October 17 the user
explicitly accepted this small Survival difference and directed work toward
7 MiB without game-content compromises. It is no longer a blocker for this
candidate. This does not establish the performance of future compressed builds.

## Still required

Test real cartridge revisions/firmware, actual model 1/2/3 consoles,
cart-menu return, audio, and saves across full power cycles.

The 7 MiB and 8 MiB cartridge families still need a substantially smaller build.
The separate streaming experiments preserve their tested data exactly but do
not yet meet that size target or qualify a playable compressed build.

Full measurements and hashes are retained in
[compatibility-candidate.json](compatibility-candidate.json). Decoder experiments
are documented in [the experiment notes](../experiments/everdrive_streaming/README.md).
