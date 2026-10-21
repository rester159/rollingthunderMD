# v0.91 Beta — implementation and validation

Local work responding to [Captain Past's graphics/audio report](https://itch.io/post/17656677)
and the [v0.9 devlog feedback](https://rester159.itch.io/rolling-thunder-md/devlog/1717605/rolling-thunder-md-v09-beta).
The tested candidate has been promoted unchanged to v0.91 Beta; see
[release notes](RELEASE_0_91.md) and [devlog](DEVLOG_0_91.md).

## Fixes

- **Save-slot labels:** each queued row now keeps its own text buffer until
  composition finishes. The three rows render SLOT 1, SLOT 2 and SLOT 3.
- **Slow title cast:** idle menus no longer repeatedly validate the SRAM journals.
  Settings are saved when they change and when starting a session. The measured
  title cadence increased from 300 to 596 updates per 600 NTSC refreshes.
- **Stray palette pixels:** title/intermission palettes transfer only changed
  entries, two per horizontal blank. The VDP consumes both queued words before
  visible pixels begin. Sampled audio keeps playing from its RAM buffer while
  the palette routine reserves the cartridge bus; nested DMA preserves that
  reservation through the next VBlank graphics upload. If the main loop misses
  publication, the early scanline interrupt releases the reservation.
- **Yellow sound-test flash:** the music driver is installed while the title is
  blank. Menu background restoration uses bounded VBlank DMA, avoiding long
  sequences of map writes that could overlap the palette interrupt.
- **Attract discovery:** leaving the Arcade submenu idle for ten seconds starts
  the existing attract sequence. Home and option pages do not time out.
- **Rushed music opening:** the Z80 discards music ticks accumulated while
  initializing a new track and its first instruments. The opening notes retain
  their source rhythm instead of rapidly consuming a startup backlog. Sample
  playback counters and buffers are preserved.
- **October 19–20 gameplay feedback:** opaque black open-door interiors, the
  arcade bullet wall-bit mask and the missing yellow-mutant wall response are
  fixed. The Stage 1 supply door and the tester's V2-clone retest were also
  checked. See [the detailed feedback review](V0_91_OCT20_FEEDBACK.md).
- The initial selection footer identifies v0.91 BETA.

## Validation

The local candidate remains **4,194,304 bytes / 32 Mbit**. The normal build
reproduces all ten updated stage banks and validates the packed artwork and PCM spans.
No gameplay assets or audio samples were removed for these changes.

Artifact: `dist/Rolling_Thunder_MD_v0.91_Beta.bin`.

All 16 feature regression groups and 22 timing/menu/pacing checks passed, plus
the Z80 DMA handoff, nested audio-reservation and PAL/NTSC music-start rhythm
checks.

Candidate SHA-256: `15bbd465776b3b875896ebafd06166bdc3f9fa161977658a931949d2d7515675`.

Reports are in [validation/v0.91](validation/v0.91). Each ROM-based report records
the tested hash. PAL timing probes change only the region header to `E`;
their ROM hash therefore differs from the normal build.
Reports for the earlier candidate are retained in `prior-candidate/`.

- Controller-driven menu tests check the actual rendered slot digits, title
  cadence, forty sound actions, idle attract entry, Start handoff and Home idle.
- Diagnostic Genesis Plus GX traces check the VDP's actual FIFO consumption
  slots, complete lower palettes, audio bus holds and scene deadlines.
  A CPU port-write timestamp alone does not establish when CRAM is updated.
- Save restoration and frontend controls are exercised on Genesis Plus GX and
  PicoDrive. Additional checks cover Arcade switches, all thirteen sound-test
  tracks, two complete attract loops and all nine intermissions.
- The deterministic Stage 2 opening workload compares 900 simulation updates
  with v0.9, separately for solo and co-op Campaign. This is a limited workload,
  not a full campaign performance guarantee. Solo remains 900 updates and 900
  presentations in 952 refreshes; co-op now records 900 updates and 742
  presentations in 929 refreshes, versus v0.9's 741 in 928. The corrected bullet
  behavior changes this workload; details are in the October 20 review.

## Still open

- Captain Past supplied the requested track details and a comparison recording.
  See [the audio follow-up](V0_91_AUDIO_FEEDBACK.md). The startup timing bug is
  fixed locally; track 7 headroom, later Main BGM 1 service stalls, instrument/
  percussion fidelity and the faint high tone remain open. Repeated captures
  still show intermittent track 7 clipping. No gain or instrument changes have
  been made. The reply monitor is paused now that the clarification has been
  received, recorded and investigated.
- Broad door/cave hue matching remains a separate visual review; the specific
  green-interior defect is fixed. Leila's restored hair colors and shared enemy
  colors have been retained.
- The original V2-clone reporter confirmed v0.9 works on their Model 1 VA7 /
  V2 OS3.6 clone and V3 Pro. Other untested hardware remains unverified.
- The palette fix still needs a real-console retest, particularly on the two
  systems used for the report.

Publication and source release are tracked separately from these validation results.
