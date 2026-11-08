# v0.81 local candidate — unreleased

These changes address the October 18 itch.io feedback and the requested opening
screen credit. The user requested no release. The published v0.8 ROM and
`CURRENT_BUILD.txt` remain unchanged; the test ROM lives in
`dist/comments-fix-candidate/`, excluded from Git.

## Changes

- Initial Arcade/Home screen: `PORTED BY RESTER159, 2026` at the bottom and
  `v0.81` at bottom right. Other menus restore the background beneath the footer.
- Character selection retains its portrait graphics and updates the selection
  frames during VBlank, eliminating the black flash on switching/joining/leaving.
- Home Settings → Controls offers independent P1/P2 layouts. Up/down selects the
  player; left/right changes the layout. Default is A/C jump and B fire; the
  alternatives are A jump with B/C fire, or B jump with A/C fire. These bindings
  also apply in Arcade and Survival. Menu navigation remains unchanged.
- Save format v2 stores both layouts and accepts existing v1 saves with default
  controls. A changed setting waits for VBlank while the title animation runs,
  preventing a missed palette interrupt from freezing the menu.
- HUD atlases use transparent pixels in place of their opaque black backing.
  All glyph colors and attributes remain intact. Conversion is stored in the
  artwork; level loading uses the original bulk upload without extra processing.
- The Z80 DMA handoff rechecks the protection request after clearing its previous
  acknowledgment. This closes a race that could let music read ROM during a new
  graphics transfer. It addresses a reproduced synchronization fault; physical
  hardware confirmation of the reported audible skip remains outstanding.
- A cartridge mapper probe displays an explanation if the required bank switching
  is unavailable, instead of proceeding into invalid banked assets.

## Scope and remaining feedback

This is still a 7 MiB (56 Mbit) ROM requiring compatible SSF2 banking. It does
not add support for cartridges limited to 4 MiB (32 Mbit). A 64 Mbit cartridge
has 8 MiB of capacity, but still needs compatible banking. Launch complaints without emulator or
cartridge details cannot yet be tied to a specific failing platform. Neither
physical console testing nor universal cartridge compatibility is claimed.

This earlier comments-fix candidate does not include attract mode. Subsequent
unreleased attract work is documented in [Arcade attract](ARCADE_ATTRACT.md).
A separate arcade intro, additional itch.io tags, and suggestions for other
ports are not implemented by these bug fixes. No itch.io edits, replies, uploads,
Git pushes, or release publication are part of this local candidate.

## Validation

The local build folder holds matching symbols, build metadata, and individual
reports. Tests cover character selection, actual P1/P2 jump/fire behavior, repeated
settings changes, save migration and corruption recovery, cold and warm resets,
regional cartridge starts, supported/unsupported mapper behavior, the actual Z80
race interleaving, door pixels and animation, and all ten stage/high-score loaders
with finite music completion. Timing samples compare the same opening workloads
with the v0.8 door-fix build; they are not full-game benchmarks.

Final local candidate SHA-256:
`f6323cd1b4e0afb3b3c5b08c068b350f7d185640c9ce7b8145bbe360b5270ede`.
Size: 7,340,032 bytes — 7 MiB / 56 Mbit in cartridge terminology.

All 15 emulator test jobs passed, including 60 soft resets, nine regional boot
cases, nine door-animation cases, ten stage/audio cases, both cold-save suites,
and both frontend suites with 30 repeated settings commits each. The host save
suite also passed v1 migration, v2 layout persistence, corruption fallback, and
all 65 interrupted-write positions. Final scene captures preserve the world,
all previously visible HUD ink, and the high-score screens exactly across all
ten stages. Source audit and whitespace checks passed.

Opening timing samples (about 30 seconds, NTSC GX; v0.8 → local candidate):

| Mode | Updates/sec | Presentations/sec |
| --- | --- | --- |
| Campaign 1P | 58.425 → 58.358 | 58.425 → 58.358 |
| Campaign 2P | 58.525 → 58.491 | 58.491 → 58.491 |
| Survival 2P | 49.969 → 49.969 | 47.572 → 46.773 |

Campaign's longest measured reload stalls remain 21 refreshes (1P) and 20 (2P),
matching v0.8. Survival's presentation rate is lower in this sample; its update
rate is unchanged. These results do not certify physical hardware performance.
