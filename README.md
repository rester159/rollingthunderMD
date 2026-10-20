# Rolling Thunder MD

Native Mega Drive / Genesis port of Rolling Thunder, updated October 19, 2026.
The current version is **v0.9 Beta**, packaged as a **4 MiB / 32 Mbit** linear ROM.

Local ROM: `dist/Rolling_Thunder_MD_v0.9_Beta.bin`. See
[CURRENT_BUILD.txt](CURRENT_BUILD.txt), [release notes](docs/RELEASE_0_9.md), and
[dev update](docs/DEVLOG_0_9.md). ROM binaries remain excluded from Git.

Includes Arcade and Home, solo/two-player Campaign and Survival, both agents,
all ten campaign areas (five stages followed by their harder second-story
versions), Arcade attract, three SRAM Campaign save slots, title-screen Continue,
Arcade options/DIP switches, and a transparent in-field HUD. Home displays
pistol and machine-gun ammunition separately. Leila retains her original hair
colors, with lipstick removed from her gameplay sprites.

See [Home/Arcade feature details](docs/HOME_FEATURES.md) and the
[optimization notes](docs/THIRTY_TWO_MBIT_PROGRESS.md). Earlier candidate reports
retain their original dates and hashes; current-version checks are recorded in
`docs/releases/0.9/`. This beta has emulator validation, not a physical-hardware
certification or a guarantee of 60 FPS in every scene.

In lighter 2P scenes, enemies use their full banked poses. Crowds, rendering lag
or sprite-memory pressure select compact poses; full detail returns after 30
light simulation updates. 1P keeps its existing animation policy.

## What is and is not included

This repository contains the native engine, converted **graphics/layout tables**
(in C and JSON), build tools, and a small vendored offline audio synthesizer.
It is **not an asset-free repository**. The native art includes the existing
Leila/RT2 adaptation and project-specific artwork; no RT2 executable is included.

No original ROM archives, executable ROM images, built `.bin` files, extracted
voice samples, or rendered music clips are included. You must supply your own
**Rolling Thunder revision 3 arcade `rthunder.zip`** to regenerate the audio.
There are no ROM downloads in the build process. Input chip hashes are checked.
The Sega splash/chant and Namcot splash/logo are omitted entirely; the binary
boots directly into the game menu. Cartridge-header identifiers are format metadata, not logo assets.

## Build

Tested on macOS with:

- Python 3.12+ and NumPy (see `requirements.txt`)
- SGDK **2.12**, built with bank switching enabled
- GNU m68k-elf GCC **16.1.0** and binutils **2.46.1**
- A host C/C++17 compiler (`cc` / `c++`), GNU Make, and Java for SGDK's rescomp

The frozen bank checks require the tested cross compiler: generated lookup
functions and fixed completion-bank exports are layout-sensitive. A different
compiler may fail the bank checksum checks rather than silently producing an
untested layout. SGDK and its cross compiler are external dependencies, not
bundled here. Set `GDK` to the directory containing `makefile.gen`, `inc`, `lib`
and the `bin/m68k-elf-*` tools. Put a suitable `java` on `PATH`.

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements.txt
export GDK=/path/to/sgdk
python tools/build.py --rom /path/to/your/rthunder.zip --sgdk "$GDK" \
  --layout packed --pose-codec refined --cart standard --output-dir dist/v0.9-beta
```

The flags above select the current 32 Mbit build. The unqualified `make build`
target retains the older shared-layout defaults.
Do not put ROMs in Git. `roms/`, `.build/`, `dist/` and common ROM/archive formats
are ignored. The build reads the supplied archive; it never modifies it.

Outputs:

- `dist/v0.9-beta/RollingThunderMD.bin` — 4 MiB / 32 Mbit linear cartridge
- `dist/v0.9-beta/SHA256SUMS` — binary checksum
- `dist/v0.9-beta/build.json` — build identity and validation metadata

All intermediates are generated under `.build/`. No research checkout, previous
Mega Drive ROM, local extracted-audio directory, emulator, or network download is
needed to build. `make clean` removes only generated build/output directories.
`CC` and `CXX` can select the host compilers; `--jobs` controls stage compilation.

The audio build decodes the arcade samples, converts the source music records,
and synthesizes the finite ending/high-score clips with pinned ymfm source.
Every stage bank and finite audio clip is checked against the frozen baseline.
Native runtime and Z80 drivers are compiled from source, then the cartridge
banks are assembled from those new outputs. The compact packer then checks all
four audio placements with the runtime planners and independently verifies linked
pose pixels, geometry, background maps and world collision data.

## Older build profiles and validation history

The following larger layouts are retained for comparison; they are not the
v0.9 Beta download. Use the explicit packed/refined command above for v0.9.

The default profile is tested in PicoDrive and Genesis Plus GX with standard
SSF2 banking and SRAM. Build the separate **Mega EverDrive Pro** profile with:

```sh
python tools/build.py --rom /path/to/your/rthunder.zip --sgdk "$GDK" --cart mega-everdrive-pro
```

This adds `dist/RollingThunderMD-MegaEverDrivePro.bin` and
`dist/build-mega-everdrive-pro.json`. It uses extended SSF banking and banked
battery RAM, and leaves physical bank 31 reserved for saves. Both profiles are
15.5 MiB. **The Pro profile has software protocol tests, but no physical-cart
validation.** The image does not fit Core/X3/X5, and these default profiles do
not reserve the v2/X7 OS area. See the [hardware matrix](docs/HARDWARE_COMPATIBILITY.md).

The compact build shares identical raw graphics and geometry and packs map
blocks. It preserves enemy counts, animation policy and audio; it does not
introduce runtime graphics decompression. All linked pose pixels and packed
geometry are compared with the frozen reference during every build. Use
`--layout frozen` only to build the older 32 MiB layout for comparison.

An **experimental 15 MiB candidate** for the Mega EverDrive v2/X7 protocol is
available separately:

```sh
python tools/build.py --rom /path/to/your/rthunder.zip --sgdk "$GDK" \
  --layout shared --cart mega-everdrive-v2-x7 --output-dir dist/compatibility-candidate
```

This shares immutable geometry, duplicate pose tables and identical raw graphics
across stages. Graphics and audio remain uncompressed; gameplay modes and
animation policies are unchanged. Pixel data retain their original offsets within
512 KiB banks, preserving the original DMA split boundaries. The image occupies
banks 0–29 and uses only the documented upper portion of bank 31 for saves,
leaving the OS reservation intact. See
[Krikzz's extended-SSF specification](https://krikzz.com/pub/support/mega-everdrive/x3x5x7/dev/extended_ssf-v2.txt).
This is a candidate, not physical-hardware certification or support for
7 MiB cartridges. The standard shared-layout variant is selected with
`--layout shared --cart standard` for emulator comparisons.
See the [candidate results and remaining hardware checks](docs/COMPATIBILITY_CANDIDATE.md).

A **7 MiB (56 Mbit) candidate** combines lossless graphics storage with
sample-exact Z80 audio streaming. It uses 7,337,646 bytes before cartridge-bank
padding (7,340,032 bytes on disk), preserving every mode, character and asset.
Campaign opening performance has recovered to the accepted build's range;
measurements include a separate jumping workload and both protagonists.
Build it with `--layout packed --pose-codec fast --cart standard --output-dir dist/seven-mib-candidate`. It has not replaced the accepted releases,
and its physical cartridge/console compatibility is still unverified.
See [the size, timing measurements and validation report](docs/SEVEN_MIB_PROGRESS.md).

Version 0.81 meets the **32 Mbit = 4 MiB = 4,194,304 byte** target using the
refined pose codec and compressed audio. Build with `--layout packed --pose-codec
refined --cart standard`. The older 7 MiB and extended-mapper measurements above
are historical. See [feedback validation](docs/ITCH_FEEDBACK_FIXES.md) and
[Arcade attract mode](docs/ARCADE_ATTRACT.md). The user reported v0.81 working on a PAL clone with a Chinese EverDrive V3;
v0.9 itself has not been tested on that hardware. KFusion and Mednafen remain
unverified.

Standard Genesis controller inputs are used; Home's setup lets
player two join before choosing the characters.

The historical adaptive baseline passed 48 opening performance runs, 20 stage/checkpoint
entry probes, 3,550 matched gameplay-state comparisons and 862 unchanged 1P
image comparisons. Average 2P displayed rates were 54.49 FPS in Campaign and
43.09 FPS in Survival. These include presentations reusing artwork; they are not
fresh-animation rates or a guarantee of 60 FPS. Restoring light-scene animation
costs about 1–1.5 FPS compared with permanently compact detail.

**Start pauses/resumes gameplay** in Arcade and Home. Either controller can
pause a 2P game; controller two is ignored for pause in solo. The screen dims,
gameplay and its timer freeze, and music continues. Menu confirmation and
Continue retain their existing Start behavior.

**Battery-backed SRAM** stores the best score, the five-record initials table,
starting credits and the sound test's PSG mute setting. Settings save when
changed; scores save at life/stage boundaries, game over, completed initials
entry and when you pause. Pause before powering off to save an ongoing run's
best score. Progress/lives are not resumed from SRAM. Empty or invalid SRAM
uses defaults; two CRC-checked slots protect against interrupted writes. Enable
battery saves in your emulator and retain its `.srm` file. Soft reset returns to the menu and restores the last committed save, including
after playing or pausing. The default profile uses standard SSF2 banking and odd-byte SRAM at
`$200001–$20FFFF`. The Pro profile uses a separate banked-save implementation;
its physical persistence still needs testing.

The public build removes boot intros, updates the cartridge title, and adds
SRAM saves and Start pause. The rendered art and audio remain unchanged despite their new storage layout. See
`docs/VALIDATION.md` for checks specific to the public build.

## Source layout

- `src/`, `inc/`: runtime, Z80 drivers and native tables
- `assets/stages/`: stage data and generated lookup source, independently linked
- `assets/completion/`, `assets/ui/`: native completion/menu/high-score data
- `tools/`: owner-ROM audio conversion and portable build orchestration
- `vendor/ymfm/`: offline YM2151 synthesis source; never runs in the cartridge

## Notices

Rolling Thunder and its original characters/art/music belong to their respective
rights holders. This is an unofficial port. No license to the original game
assets is granted by this repository. Third-party source licenses and notices
are retained under `licenses/` and `vendor/ymfm/LICENSE`; see `NOTICE.md`.
