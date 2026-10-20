# Second 32 Mbit optimization pass — October 18, 2026

**Unreleased.** The payload is now **4,189,406 bytes**, a further **2,072-byte**
reduction. Spare capacity increases from 2,826 to **4,898 bytes**. The cartridge
file remains padded to **4,194,304 bytes / 32 Mbit**. The versioned v0.81 ROM,
`CURRENT_BUILD.txt` and published release remain unchanged.

Candidate: `dist/32mbit-candidate/RollingThunderMD.bin`.
SHA-256: `bfa641decaaf94721b83f41c76ebd12a715826de557216983f89c21223299237`.
The preceding candidate remains in `dist/32mbit-pass1/`, with its original
`48b3066d…4337c6` hash. A normal build in `dist/32mbit-pass2-reproduced/`
reproduces the new candidate byte-for-byte.

## Retained change

Raw sprite images can overlap matching suffix/prefix bytes at **word** boundaries;
they do not require whole-tile boundaries. The builder also shares a complete
image if it already occurs at an aligned position in the pool. Every image
remains contiguous and word-aligned for the existing DMA upload path. The raw
image pool shrinks from **365,920 to 363,848 bytes**.

There is no new runtime decompression, format or pointer indirection. All 500
raw images remain raw. No gameplay, graphics, modes, animation frames or audio
samples were removed or reduced. This pass changes build-time placement only.

A trial that grouped incoming map-tile acquisitions was discarded because some
solo samples slowed down. It is not in the retained source or candidate.

## Timing and correctness

Comparisons use the first 32 Mbit candidate, with the same scripts and emulator.
All 40 short Campaign samples across ten stages, both agents and one/two players
run without runtime faults. Changes are small and mixed: up to about two playing
refreshes in a short sample. These are simulation timing measurements, not a
claim of improved rendered frame rate.

Fixed-work comparisons also cover all 40 combinations. Twenty-two complete
**1,800 updates**; the other eighteen end the scripted session before that target
on both builds, so both versions of those cases are additionally compared at
**900 updates**. Those earlier failed target attempts are retained in the
evidence. At 1,800 updates, the difference ranges from two fewer to three more
playing refreshes. At 900 updates, differences are at most one refresh either
way. This is a storage improvement with small measured timing variation, not
evidence of a universal speedup or perfectly identical frame pacing.

- All **11,270 unique sprite images** pass exact checks, including actual 68000
  decoding of the 10,770 compressed images and byte comparisons of 500 raw images.
  Decoder cycle measurements are unchanged from the preceding candidate.
- All ten stage banks reproduce and the full build verifies every pose record,
  geometry record and background map cell against the frozen sources.
- Twelve regression groups pass: menu/controller behavior, soft reset and cold
  saves on both cores, compatibility, doors, audio handoff, mapper checks and
  completion scenes.
- All 40 stage/player/agent combinations pass stage entry, populated-cache
  verification and high-score entry. Arcade attract, exact completion-background
  VRAM and native column-copy checks pass.
- The four stored audio streams and their encoding metadata are byte-identical
  to the preceding candidate; the prior sample/cadence validation still applies.
- Four new aligned-pool tests cover partial-tile overlaps, containment,
  alignment, deterministic ordering and randomized spans. All five existing
  packed-storage tests pass.

Evidence: [`validation/32mbit-pass2`](validation/32mbit-pass2).
No physical console or cartridge testing is claimed. Build using the command
in [the first-pass notes](THIRTY_TWO_MBIT_PROGRESS.md#reproduce), with a separate
output directory. Nothing from this pass was pushed or released.
