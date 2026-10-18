# Lossless older-cartridge streaming

The selected implementation is integrated into the **7 MiB candidate**, with
sample-exact audio and native-tested graphics. See the
[current size, timing and validation report](../../docs/SEVEN_MIB_PROGRESS.md)
and [machine-readable evidence](../../docs/packed-candidate.json).
The accepted 15.5 MiB releases and separate 15 MiB v2/X7 candidate are unchanged.
Physical cartridges and consoles remain untested.

## Selected implementation

`tools/pcm_stream.py` packs four-sample groups after a 256-sample raw preload.
The builder selects the smaller absolute/common-sample or previous-sample-delta
encoding for each clip. A two-byte header bounds each 32 KiB source segment;
no encoded group crosses a segment. Source length and every sample are preserved.

`tools/pcm_mask_driver.py` generates the RTPC9 Z80 driver. Its sixteen pair paths
for each encoding have equal execution time. Previous-sample mode reads its
predictor from the last written byte of the Z80-local ring; absolute mode writes
the next byte. Both keep the original 227-cycle output period. The driver uses
only documented Z80 instructions, a 256-byte local ring and a 64-sample local
interrupt window. Threaded dispatch restores the interrupt-overwritten return
word. `src/pcm_stream_driver.c` owns the mailbox, ROM mapping and clip transitions;
it rebuilds dispatch tables when changing encoding and reloads the driver on
replacement before releasing the Z80.

`tools/packed_patterns.py` keeps frequent player and guard poses raw and stores
other images as palette-normalized byte-LZ streams. Six-byte descriptors address
shared palette prefixes and encode the used color count. `fast_decode.s` expands
only the required palette rows and decodes into the existing DMA buffer, with
no persistent image dictionary. Canonical and fast diagnostic entry points share
the `(colors << 16) | raw_bytes` argument format; zero colors retains the original
sixteen-row diagnostic behavior.

`tools/indexed_poses.py` and `tools/shared_geometry.py` share immutable geometry,
pose records and index arrays while preserving original animation indices.
`tools/compact_build.py` places streams in unused alignment gaps, rejects overlap
and bank-crossing streams, and refuses packed images above the 7 MiB limit.
Every original linked pose, geometry record, map and palette is verified after
assembly by `tools/verify_compact.py`.

The original renderer, frame pacer, sprite FIFO and adaptive detail policies are
retained. A short decode stall could increase later artwork reuse; keeping the
affected entry and large poses raw restored Campaign performance to the baseline
range in the measured workloads. The report includes fresh-presentation counts
and the small remaining differences, rather than treating all presentations as
new animation frames.

## Validation tools

- `canonical_probe.py`: every unique pose and five boundary fixtures through a
  standalone native decoder; checks pixels, canaries, input/output cursors,
  stack restoration and preserved registers. Its storage projections are for
  the standalone diagnostic, not the final mixed raw/compressed layout.
- `check_decoder_core.py`: equal native results and cycle counts with and without
  the host-only return breakpoint.
- `check_packed_native.py`: every final linked compressed stream with its actual
  shared palette prefix, plus byte-exact raw images.
- `trace_core.py`: creates an isolated GX recorder with host-only DAC, DMA and
  instruction-cycle observations; it adds no guest instructions or cycles.
- `check_audio_stream.py`: every actual DAC sample and interval, real ending-scene
  DMA, completion, replacement, rearm, idempotent stop and physical mapper restore.
- `tests/check_packed_candidate.py`: combined audio, scene, save, reset, region,
  launch and timing regression checks against one ROM hash.

The current reference core is the user's installed Genesis Plus GX libretro
core; the instrumented build is local and separate. Build it with
`trace_core.py --source /path/to/Genesis-Plus-GX --output /path/to/trace-core`.
All tools expose their required paths through `--help`. The accepted baseline
is read-only. No arcade archive or generated source artwork is distributed here.

## Earlier experiments

`results.json`, `codec.py`, `decode.s`, `pcm_mask.s`, `audio_probe.py`, `workload.py`
and `summarize.py` preserve the earlier feasibility experiment. Its byte-LZ and
block-mask storage estimates were not playable-ROM results and are superseded
by the assembled candidate above. The separate uncompressed layout is documented
in [the 15 MiB candidate report](../../docs/COMPATIBILITY_CANDIDATE.md).

Rejected RAM-stream, player-cache and allocator experiments are retained as
`.txt` references. They are not compiled into the selected game. In particular,
the Z80 does not read a 68000-main-RAM ring, and no speculative extra heap buffer
or LRU sprite allocator was adopted.
