# HUD, Arcade options and Home SRAM — v0.9 Beta

Updated October 19, 2026. These features are included in v0.9 Beta.
Current ROM: `dist/Rolling_Thunder_MD_v0.9_Beta.bin`.
The cartridge remains exactly **4,194,304 bytes (32 Mbit)**.
See [v0.9 release notes](RELEASE_0_9.md) for current build identity and checks.
The candidate evidence below records the earlier builds on which each feature
was developed and validated.

## Home saves

Home → Play offers **three cartridge SRAM slots** before agent selection.
Starting in an occupied slot requires an overwrite confirmation. A checkpoint
is saved when a new Campaign starts and when the next area begins. Title →
Continue selects a slot and resumes **at that area's authored entrance**.

The checkpoint keeps the solo/co-op choice, both agent identities, shared credits,
both scores, pistol and machine-gun ammunition, equipped weapon and bonus-life
progress. Live players restart with full health; a dead co-op partner stays dead
and still needs a credit. Timer and enemies restart with the area. This is an
area checkpoint, not a mid-area suspend. Survival runs are not resumable.

SRAM also stores controller layouts, settings, records and Arcade switches. No emulator savestate or external account is needed.
If SRAM is unavailable, the menu explicitly offers play without saving.
Flash cartridges may require their normal reset/menu procedure to copy battery
RAM to an SD-card save file; the ROM writes cartridge SRAM directly.

Existing settings/records retain their original two 64-byte records at offsets
0 and 64. New data occupies offsets **256–767**: three independent checkpoint
journals plus one profile journal, each using two 64-byte records. CRC32,
generation counters and commit-last writes retain the preceding valid record
if a write is interrupted. Unchanged payloads do not rewrite SRAM. SRAM access
halts ROM consumers and happens after pending video transfers finish.

The warm-reset wrapper imports the committed SRAM mirror and then restores the
initial stack before entering the SDK runtime. Keeping the larger temporary
mirror on that stack caused a death/restart collision; that defect is fixed.

## Arcade

Arcade now opens **Start / Options / DIP Switches**. Start retains the existing
arcade attract sequence and Start-to-play handoff. Options exposes independent
P1/P2 controller layouts, the existing 13-track sound test and Reset DIP Defaults.

DIP Switches: Up/Down selects a setting; Left/Right or A changes it; B returns.
Settings persist in SRAM and apply to Arcade, independently of Home rules.

| Setting | Default | Alternative |
| --- | --- | --- |
| Lives | 3 | 5 |
| Bonus life | 70,000 / 200,000 | 100,000 / 300,000 |
| Timer | 150 | 120 |
| Difficulty | Normal | Easy |
| Invulnerability | Off | On |
| Demo sound | On | Off |

These are the gameplay-relevant choices from the revision 3
[MAME DIP definitions](https://github.com/mamedev/mame/blob/master/src/mame/namco/namcos86.cpp).
Coinage, cocktail cabinet, service and level-select hardware switches are not
implemented as console menu options. Easy selects the alternate authored enemy
grid: the original CPU1 `$4253` selector is the filtered DSWB difficulty bit
(after the MCU's interleaved DIP wiring). Invulnerability protects against
combat damage; it does not suspend the timer or alter stage progression.

## Menu colors

Main, Home and Arcade menus use light gray/white letters with a dark one-pixel
shadow, matching the supplied arcade reference. The existing console palette
provides both inks across its raster split. Background/cast palettes, menu
geometry, sprite counts and tile allocation are unchanged.

## HUD and performance

The HUD is a transparent sprite overlay over both scrolling background planes.
It retains the original glyphs and colors. No Window plane bars or mid-screen
palette replacement are used. All ten per-stage fonts share one lossless
monochrome atlas; only changed glyphs are colored and transferred.

HUD sprites reserve 22 SAT entries. The renderer keeps its existing compact
co-op pose policy when sprite pressure is high; actors and game logic are not
removed. The fixed HUD tail links to actor entry four. The actor sort exchanges
its head into that entry and repairs one link, avoiding a second per-frame DMA
command. Both SAT publication buffers receive the fixed HUD prefix once per
area, avoiding its per-frame copy. Score calculations overlap the preceding video
publication; SRAM commits wait for that publication to finish.

The preceding `home-features-candidate` build (SHA-256
`c08a333de4045cd7ed3966409f3322a9d59aa27ea5ba06dae7da7f585cffde55`)
provided these measured 900-update NTSC samples compare against the preceding local
`solo-ammo-candidate` build, not the older published release:

| Workload | Prior play refreshes | Candidate play refreshes | Prior / candidate presentations |
| --- | ---: | ---: | ---: |
| Area 1 solo | 902 | 902 | 899 / 899 |
| Area 2 solo | 900 | 900 | 898 / 898 |
| Area 1 co-op | 902 | 902 | 899 / 899 |
| Area 2 co-op | 924 | 925 | 743 / 743 |

Solo and the lighter co-op sample match during play. The busy Area 2 co-op
sample takes one additional refresh per 900 updates (about **0.1%**); it is
not evidence of zero overhead or a 60 FPS guarantee. Area reloads take roughly
two additional refreshes to construct the transparent HUD. These are bounded
emulated workloads, not full-game or physical-hardware benchmarks.

## Validation

Prior HUD/save feature evidence is under `docs/validation/home-features/`.
The current build is tested separately under `docs/validation/no-achievements/`,
with each report identifying its ROM hash. Prior reports describe earlier builds.

- Genesis Plus GX and PicoDrive: all three slots, separate characters/inventory,
  reset and fresh-process SRAM restoration, settings persistence.
- Host SRAM tests: independent journals, all 65 interrupted-write points,
  corruption fallback and warm-reset restoration; dead co-op partners cannot
  gain a free life through a checkpoint.
- All ten stage loaders in solo and co-op, plus high-score entry; bounded
  movement/shooting workloads across every area and 2P Survival.
- PAL/NTSC Area 2 AMMO doors, including an active machine gun, cancelled entry,
  retries and spent-door behavior.
- All 950 HUD glyph variants preserve their original colored pixels. Static
  area captures preserve every playing-field pixel and existing HUD glyph while
  exposing scenery in the formerly opaque bands.
- 20,000 adversarial sprite batches preserve every actor and priority/tie order,
  including empty batches, with one SAT transfer.
- Actual menu controller layouts, Arcade DIP consumers, muted attract versus
  audible gameplay, default reset, and two complete attract loops.

No physical-console or flash-cartridge validation has been performed for v0.9.
The earlier v0.81 hardware report does not certify this beta.
