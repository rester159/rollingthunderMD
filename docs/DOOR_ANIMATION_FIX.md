# v0.8 Alpha — arcade door animation correction

Opening a door previously changed its artwork into other door types. The arcade
CPU2 tables at `CF80`, `CFAF` and `CFD4` select the first, second and third
animation frames; each table is indexed by door kind. The old art generator
transposed those two indices. The stored pixels and DMA transfers were intact,
so byte comparisons against the old generated artwork did not catch the error.

The corrected descriptor sequences are:

| Door kind | First frame | Second frame | Third frame |
| --- | --- | --- | --- |
| 0 | `7667` | `7672` | `767D` |
| 1 | `770C` | `7717` | `7722` |
| 2 | `77B1` | `77BC` | `77C7` |

Both the resident and streamed door atlases were regenerated for all ten stages
from the owner-supplied revision 3 arcade set. Door colors use the correct kind's
lookup and the existing Mega Drive palette fitting. Fan art, streamed geometry,
door timing, gameplay, audio and the renderer are unchanged. The corrected
resident atlas shares two more identical components and drops from 272 to 256
tiles. The stage bank fingerprints were repinned for these intentional art edits.

## Current artifact

- Local ROM: `dist/Rolling_Thunder_MD_v0.8._Alpha.bin`
- SHA-256: `e30b81671e1277b2ad586be717fb4420085d9971e37813c97a4cef2668b7e7ed`
- Used data: **7,332,772 bytes**, leaving **7,260 bytes** below the 7 MiB limit.
- Padded cartridge file: **7,340,032 bytes (7 MiB)**.
- Matching build and validation metadata: `dist/door-fix-candidate/`.

The previous 7 MiB candidate remains in `dist/seven-mib-candidate/`. The accepted
15.5 MiB release files in the separate original checkout were not modified.
ROMs and boot-intro assets remain excluded from Git.

## Validation

`tests/test_door_animation.py` reproduces the old failure on the opening door's
second rendered frame. With the correction, all three door kinds pass in Arcade,
Home 1P and Home 2P: nine cases, 216 published door-frame checks, and the complete
opening/closing sequence in every case. The fixture seeds only the door kind
and open request at an authored doorway. It checks the real renderer's published
VRAM pixels, palette attributes, component positions and sprite list against
arcade-derived art, rather than inspecting a frame still being prepared in RAM.

The full packed-candidate suite also passes: finite audio and transitions,
all ten stage/high-score loaders, soft resets, cold saves, cartridge launches and
six timing workloads. All 11,270 shared poses pass native or byte-exact checks;
the full build verifies 33,720 pose records, 29,790 packed geometry records,
808,960 background cells and 2,899,006 original audio samples. The measured
update/presentation counts match the previous 7 MiB candidate in all six timing
workloads; Leila has one additional full-art presentation in place of a reused
presentation. Physical console and cartridge testing remains outstanding.

See [machine-readable evidence](door-animation-fix.json) for the ROM identity,
individual door results and stage-bank fingerprint changes.

## Reproduction

The converted C art is included. Normal builds need no additional generation
step. To regenerate the door assets from the supplied archive:

```sh
python tools/build_door_art.py --rom /path/to/rthunder.zip
```

Then use the documented packed build and test with its matching symbols:

```sh
python tests/test_door_animation.py \
  --rom dist/door-fix-candidate/RollingThunderMD.bin \
  --symbols dist/door-fix-candidate/RollingThunderMD.symbols.txt \
  --arcade-rom /path/to/rthunder.zip \
  --core /path/to/genesis_plus_gx_libretro.dylib \
  --output .build/door-fix-tests.json
```
