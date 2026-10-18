# 7 MiB candidate — October 17, 2026

The initial candidate documented below is retained as a reference. The current
v0.8 Alpha includes the [arcade door-animation correction](DOOR_ANIMATION_FIX.md),
with a new ROM hash and fresh validation results.

**A complete, software-validated 7 MiB candidate is available.** Audio streaming
is integrated and preserves every original sample at its original cadence.
The earlier Campaign slowdown has recovered to the accepted build's range in
the measured workloads. No modes, characters, stages, animation frames, artwork,
collision data or recorded audio were removed. The accepted releases remain
unchanged. Physical cartridge and console testing is still outstanding.

## Artifact

`dist/seven-mib-candidate/RollingThunderMD.bin`

- Used data: **7,337,646 bytes** (2,386 bytes below the 7 MiB capacity limit).
- File size after required cartridge-bank padding: **7,340,032 bytes = 7 MiB**.
- SHA-256: `54468d3329e0bc07e4593d6f21cffb3f2dcf448b1bcdbe354ac3d54d5c535220`.
- Profile: standard SSF2 banking and odd-byte SRAM.
- The full production build reproduced the checkpoint candidate's exact hash.

Matching symbols, build metadata, checksums and `validation.json` accompany the
ROM. This is a capacity fit and software-tested candidate, not a claim that all
older EverDrives or Genesis Models 1/2/3 have been tested. “7 MiB” here means
7,340,032 bytes, not seven million decimal bytes.

## Audio streaming

The builder chooses between two lossless encodings per clip. One stores common
absolute sample values; the other stores unchanged/-1/+1 predictions and uses
an original absolute sample for every other value. Neither method quantizes,
resamples or changes the source. The first 256 samples are preloaded, and the
Z80 decodes the rest from ROM into its own 256-byte ring.

| Clip | Original samples | Encoded bytes | Encoding |
| --- | ---: | ---: | --- |
| Victory | 76,755 | 53,106 | Previous-sample prediction |
| Ending | 584,641 | 307,395 | Previous-sample prediction |
| Cold high score | 1,118,805 | 291,340 | Common absolute samples |
| Warm high score | 1,118,805 | 282,450 | Common absolute samples |

The encoded clips total 934,291 bytes before cartridge-placement padding.
All **2,899,006 original samples** match. Native playback maintains **227 Z80
cycles / 3,405 master cycles** between DAC writes, including source-bank changes
and the ending scene's real graphics DMA. There are no main-RAM streaming reads
from the Z80 and no new 68000 audio staging allocation.

A 64-sample local interrupt window lets the Z80 continue playback while the
68000 transfers graphics. The threaded decoder restores the word overwritten
by an interrupt's return address. Replacement reloads the driver before
restarting, and rearm selects the next clip's decoding table under bus ownership.
Start, completion, replacement, rearm, repeated stop and actual ROM-window
restoration all pass on the selected ROM.

## Graphics storage and Campaign timing

Identical geometry and pose records share storage. Index tables preserve every
original animation index. Compressed images use independent byte-LZ streams
and shared palette prefixes; the decoder only builds color-table rows actually
used by the image. Six-byte descriptors retain the stream address, color count
and palette offset. Alignment gaps between immutable ROM sections store more
compressed data without consuming another cartridge bank.

Frequently used player movement, firing, jumping and hit poses, plus guard
entry, movement and selected large poses, remain uncompressed across all stages,
costumes and color variants. This is a storage decision: the other frames are
still present and decode to exactly the original bytes. The selected build has
1,855 raw images and 9,415 compressed images. Compressed output uses the existing
DMA buffer and survives until its normal queue flush.

Tracing showed that a brief decode stall could leave the existing frame pacer
behind and increase later artwork reuse. Keeping the affected guard entry and
large poses raw removed that sustained penalty. The renderer, enemy counts,
physics, FIFO sprite allocation and adaptive animation/detail policies remain
unchanged. Experimental cache, allocator and visibility changes that did not
help were removed.

Each test covers 1,800 NTSC refreshes (about 30.04 seconds). These counts include
automatic continues and loading. They are not sustained active-play FPS for an
entire game. A full presentation refreshes pose selection; a reused presentation
retains existing artwork while updating geometry, as in the accepted renderer.

| Workload | Accepted updates / presentations | Candidate updates / presentations | Accepted full presentations | Candidate full presentations |
| --- | ---: | ---: | ---: | ---: |
| Campaign 1P, Albatross | 1,735 / 1,735 | 1,736 / 1,736 | 1,291 | 1,466 |
| Campaign 2P | 1,737 / 1,737 | 1,737 / 1,737 | 1,454 | 1,458 |
| Campaign 1P, Leila | 1,736 / 1,736 | 1,736 / 1,736 | 1,471 | 1,465 |
| Campaign 1P, jumping | 1,733 / 1,733 | 1,732 / 1,732 | 1,316 | 1,302 |
| Campaign 1P, start aligned to update 512 | 1,734 / 1,734 | 1,735 / 1,735 | 1,200 | 1,274 |
| Survival 2P | 1,502 / 1,403 | 1,502 / 1,395 | 450 | 463 |

Campaign opening presentations are 57.79 FPS in solo and 57.83 FPS in co-op,
versus 57.76 and 57.83 in the accepted build. The jumping workload is one update
and presentation lower over the whole sample: 57.66 versus 57.69 FPS. This is
not a claim of identical frame timing everywhere. Survival keeps the same
simulation-update count and presents eight fewer frames in this sample; the
user accepted Survival presentation-rate differences.

## Validation

- The production build reproduces all ten frozen stage banks and original PCM.
- Linked data verifies 33,720 original pose records, 29,790 packed-geometry records,
  808,960 map cells, collision data and palettes.
- Every final compressed image passes native 68000 decoding against the original
  pixels, with output canaries, stack and preserved-register checks. The tested
  native routine also matches the final linked game code byte-for-byte. Every raw
  image matches byte-for-byte: 11,270 images total.
- Five decoder boundary fixtures have identical results and cycle counts in the
  unmodified and instrumented emulator cores. Both codec ABIs were checked.
- All ten stage-entry images and ten high-score images match the accepted ROM.
- All four audio recordings have exact samples and cadence. The ending and
  victory checks run real scene DMA; the cold high-score check uses real entry.
- Thirty soft-reset/save checks pass across Arcade, Campaign 1P/2P and Survival
  1P/2P. Cold-load persistence and corrupt-slot fallback also pass.
- Nine launch/region cases pass across Genesis Plus GX and PicoDrive, including
  dirty launch, P2 pause/resume and reset/restart coverage.
- All six performance workloads report zero compressed-graphics buffer failures.
- Seven PCM-format and packed-storage unit tests pass, covering sample values,
  bank boundaries, palette prefixes and overlapping immutable streams.
- Both original accepted ROM hashes are unchanged.

Scene transitions are diagnostic calls into the unmodified candidate, not a
complete natural campaign playthrough. Emulator checks do not certify physical
FPGA/firmware behavior, cartridge-menu save backup, electrical timing or console
revisions. Capacity alone also cannot establish support for every older mapper.

## Reproduce

Use the owner-supplied archive and the Python/SGDK setup in the main README:

```sh
python3 tools/build.py --rom /path/to/rthunder.zip --sgdk /path/to/sgdk \
  --layout packed --pose-codec fast --cart standard \
  --output-dir dist/seven-mib-candidate
python3 tests/test_pcm_stream.py
python3 tests/test_packed_storage.py
```

`tests/check_packed_candidate.py --help` describes the combined runtime checks.
It requires the two emulator cores, the instrumented DAC recorder and an accepted
scene-reference report. `experiments/everdrive_streaming/check_packed_native.py`
checks the actual assembled pose streams and palette prefixes against the frozen
reference. See [the streaming implementation notes](../experiments/everdrive_streaming/README.md).

The measurements, hashes and detailed scope are retained in
[packed-candidate.json](packed-candidate.json).
