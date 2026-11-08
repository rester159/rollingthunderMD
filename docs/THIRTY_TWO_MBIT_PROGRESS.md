# 32 Mbit optimization candidate — October 18, 2026

**Latest:** the [second optimization pass](THIRTY_TWO_MBIT_PASS2.md) saves another
2,072 bytes without adding runtime decoding. Its payload is **4,189,406 bytes**,
leaving **4,898 bytes** within the 32 Mbit capacity. It remains unreleased.
The first-pass evidence below is retained as the comparison baseline.

## First optimization pass

The first optimization candidate fits **32 Mbit = 4 MiB = 4,194,304 bytes**.
Its payload uses **4,191,478 bytes**, leaving **2,826 bytes** before cartridge
padding. This candidate is **unreleased** and has not replaced the separately
released v0.81 ROM or `CURRENT_BUILD.txt`.

Retained first-pass artifact: `dist/32mbit-pass1/RollingThunderMD.bin`.
SHA-256: `48b3066db45e9e029e2bed8437e5f2c48826efc107949bdca6ae8aa8364337c6`.
A fresh normal build in `dist/32mbit-reproduced` produced the identical file.
This is a linear ROM with the standard SRAM profile; no ROM banking is needed.
Capacity and software checks do not certify physical cartridge compatibility.

## What changed

All modes, stages, characters, animation frames, maps and recorded samples remain.
The existing adaptive co-op rendering policy is unchanged. No audio resampling,
frame removal or gameplay simplification was used to reach the limit.

- Repeated sprite geometry and images share storage. Lossless pose compression
  preserves each original image. **500 frequently used images remain raw**,
  including player movement and all guard walking costumes, to avoid decoding
  them during Campaign play.
- All four finite PCM clips use lossless streamed phrase encoding. The Z80
  decoder preserves the original sample cadence, including scene transitions.
- Six static completion backgrounds use independently compressed blocks.
  Pointer relocation and removal of an already superseded background save
  **75,546 bytes**; animation art, maps and palettes remain intact.
- The high-score resource shares its duplicate background and 336 identical
  lookup rows, saving **83,996 bytes**. Every glyph and cell combination remains
  available. The original resource reconstructs byte-for-byte.
- Campaign reuses graphic lookups, preloads common guard poses, copies map
  columns more efficiently and groups equivalent tile-reference releases.
  These changes preserve sprite choices, map cells and tile allocation order.
  The inactive completion/high-score memory provides the larger pose workspace.

## Campaign timing

The comparison baseline is the earlier larger-ROM attract candidate,
SHA-256 `caee92199d740d37b66a3867e0293abdc48c946e8a44511cf3bb8e5803254d0a`.
These are deterministic NTSC emulator workloads, not physical hardware results
or a complete playthrough. Rates below measure simulation updates per emulated
second; they are not a claim of 60 distinct rendered frames per second.

All **40 short samples** cover the ten stages, solo/co-op, and both selected
agents. 39 match or improve on the baseline. Area 5 solo Albatross has the
remaining small difference: 822 updates / 832 playing refreshes versus 819 / 830,
about **−0.074 updates/s**. This is roughly one additional missed refresh in that
short sample, and is retained rather than rounded into a zero-regression claim.

Longer comparisons hold the amount of simulation work fixed at **1,800 updates**:

| Area / selected agent | Baseline playing refreshes | Candidate playing refreshes |
| --- | ---: | ---: |
| Area 4 / Albatross | 1,810 | 1,806 |
| Area 4 / Leila | 1,810 | 1,806 |
| Area 5 / Albatross | 1,835 | 1,829 |
| Area 5 / Leila | 1,837 | 1,828 |

Lower is faster in this table. All four longer samples improve. The eight
opening samples also match or improve their rate while actively playing.
Their total refresh windows include transitions, whose duration differs; both
the complete-window counters and playing-only counters are retained. Final-stage
short samples improve by approximately 1.85–1.94 updates/s solo and 1.07–1.27
updates/s co-op. These results show recovered Campaign performance in the tested
workloads; they do not prove identical timing in every possible encounter.

## Verification

- All ten frozen stage banks reproduce. The build verifies 33,720 pose records,
  29,790 packed geometry records and 808,960 background map cells.
- Actual 68000 decoding reproduces all **11,270 unique sprite images** exactly:
  10,770 compressed and 500 raw, with output canaries and register checks.
- All **2,899,006 recorded samples** round-trip exactly. Nine native audio tests
  cover all clips, rearming/replacement and scene playback; DAC sample values and
  the **3,405 master-cycle cadence** match exactly.
- All six completion backgrounds produce exact native VRAM pixels and maps.
  All **30,240 high-score cell/glyph combinations** and background restoration
  produce the same VRAM as the original resource.
- Stage entry, populated pose caches and high-score entry pass in all 40
  stage/player/agent combinations. Arcade attract completes two full loops,
  handles interruption, and leaves records unchanged.
- Twelve regression groups pass, including frontend, soft reset and cold saves
  on Genesis Plus GX and PicoDrive, plus compatibility, doors, music handoff,
  mapper checks and completion scenes.
- The graphic-lookup cache passes native selector comparisons in all ten stages
  with one and two players. This check used the preceding candidate with the
  same cache implementation; its recorded hash is preserved.
- Native column copying passes 4,096 cases. Host checks pass 120,000 packed
  strips, 5,000 grouped-release comparisons and 60,000 sprite-arena plans.
  PCM and packed-storage unit tests pass (4 and 5 tests respectively).
- Four 6,000-refresh two-player Survival runs cover both agents on both cores,
  including game over and return to menus, without runtime or staging faults.
  They are finite sessions, not 6,000 refreshes of uninterrupted combat.

Machine-readable evidence is retained in
[`validation/32mbit`](validation/32mbit). ROM binaries remain ignored by Git.
No physical Model 1/2/3 or EverDrive testing is claimed.

## Reproduce

With the documented SDK and the owner's arcade archive available:

```sh
python3 tools/build.py --rom rthunder.zip --sgdk /path/to/m68k-elf \
  --layout packed --pose-codec refined --cart standard \
  --output-dir dist/32mbit-reproduced
```

Use the output directory explicitly to keep this candidate separate from a
versioned release. The current source includes subsequent optimizations beyond
the separately published v0.81 snapshot.
