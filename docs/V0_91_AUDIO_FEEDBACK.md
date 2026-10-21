# Captain Past audio follow-up — 20 October 2026

Source: [Captain Past's clarification](https://itch.io/post/17658260),
posted at 14:56 UTC. Nothing has been posted or released in response.

## Report and recording

The tester connects the Mega Drive's front headphone jack to an amplifier,
normally with the console volume at maximum. Tracks 1 and 7 have loud passages
that crackle/flutter; lowering the console volume resolves that symptom.
Main BGM 1 also has reported skipping, missing percussion, instrument differences
and a faint high tone. The previously supplied hardware details were a PAL VA4
in both 50/60 Hz and another Mega Drive of unknown revision.

The [comparison recording](https://filebin.net/477lzc9w4nmyt9bt) was downloaded
for analysis and verified against Filebin's SHA-256:
`e27ed61724b468c45e486a14c25298308022a78a5cc40121cf873c77736fc47d`.
The original WAV is retained locally under `.local/v0.91/captain-past/`;
it is ignored by Git. It is 24.438 seconds, 44.1 kHz, stereo, 16-bit.
Its largest absolute sample is 0.94034 full scale, with no samples at the digital
clipping limit. That does not exclude distortion earlier in an analog signal
path, or establish the recording's console-volume setting.

## Reproduced findings

- Three starts each of tracks 1 and 7 were captured on released v0.9 and the
  previous v0.91 candidate (`6f4bfef4…`), including the initial attacks.
  Track 1 did not digitally clip. Track 7 did: v0.9 had 26, 6 and 4 clipped
  channel samples; the candidate had 48, 32 and 22. Those measurements support
  a headroom investigation, but do not identify every reported analog symptom.
- Main BGM 1 is music selector 6. Its extracted source places the first four
  melody notes at music services 0, 7, 14 and 21. The old candidate emits them
  at video frames 24, 25, 29 and 34: the opening rhythm is compressed.
- The Z80 continues counting music ticks while preparing the chip and loading
  the first instruments. It then consumes that accumulated backlog after the
  new track becomes audible. Clearing the backlog only after chip reset was
  insufficient, because the first sequencer service also installs instruments.
- A separate 1,500-refresh trace found no pitch mismatches in the observed
  normal FM melody/percussion note prefixes once source pitch fractions were
  included. This does **not** validate timbres, PSG parts or the special FM
  channel. It also found later service stalls, so the startup fix must not be
  described as resolving all of the reported skipping.
- Selector 6 assigns source parts 2, 3 and 4 to PSG. A 12,000-service source
  trace uses all eight parts; part 5, initially silent on an FM channel, enters
  at service 2101. A fixed channel swap based only on the introduction could
  harm a later passage. No channel maps or instrument patches were changed.

## Local fix and tests

The driver now starts with one immediate musical service and resets the music
tick subdivision/backlog after the first service finishes installing the track,
before its deferred FM key-ons. The DAC sample accumulator, buffered sample
data, channel maps and note data are unchanged.

`tests/test_music_start_timing.py` observes actual YM2612 key-on writes through
the diagnostic Genesis Plus GX core and uses normal menu/controller input.
It checks a start and two restarts. The old candidate fails with note intervals
of 1, 4 and 5 video frames. The fixed runtime has intervals of 7–8 frames while
retaining exactly seven music services between those notes. PAL intervals are
6–7 frames, as expected for the same music clock at 50 Hz.

Repeated output captures after the timing fix still find intermittent clipping
on track 7 (48, 32 and 0 clipped channel samples across three starts), while
track 1 stays below the digital limit. A separate single capture had no clipping,
which illustrates why one clean recording does not establish a headroom fix.

Reports for the clarification are in `validation/v0.91/captain-past/`.
The broader v0.91 regression reports identify their tested ROM hashes.

## Remaining audio work

Track 7 headroom, later Main BGM 1 service stalls, percussion/instrument fidelity
and the faint high tone remain unresolved. No volume reduction has been applied
without checking carrier levels and the resulting music/effects balance.
The headphone-path explanation for track 1 is a hypothesis, not a hardware
diagnosis. Real-console retesting is still required.
