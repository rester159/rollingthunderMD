# October 19–20 itch.io feedback — local v0.91 candidate

Reviewed the [game comments](https://rester159.itch.io/rolling-thunder-md) and
[v0.9 devlog](https://rester159.itch.io/rolling-thunder-md/devlog/1717605/rolling-thunder-md-v09-beta).
No comments, release, Git push or tag were published.

## New gameplay reports fixed

[U.S. PLATINUM INTERACTIVE DEVELOPMENT's v0.9 report](https://itch.io/post/17662773)
identified three reproducible problems:

- **Green open-door interiors.** Cleared doorway tiles were transparent and
  exposed the green stage backdrop. The affected cells now use opaque black
  tiles in all ten stages. Existing textured cave interiors, closed-door art,
  world maps, palettes and sprite colors are unchanged. Eight stages reuse
  existing black tiles; two append one 32-byte tile each. The stage bank
  manifest records the resulting reviewed banks.
- **Stage 2 shots disappearing near platforms/stairs.** The arcade's CPU2
  routine at D426 masks the material flags with bit 0. The port incorrectly
  treated every flag as a wall, including floor-change and stair metadata.
  All three native bullet-probe paths now use the wall bit. Player and enemy
  shots retain genuine wall and target collision. A controller-driven vault
  lands at world Y127 on the platform above the crates; crouching then firing
  previously caused an impact after four pixels. The candidate shot travels
  76 pixels during the tested interval without that false impact.
- **Stage 3 yellow mutants freezing/trapped in columns.** Walking handled the
  contact latch but omitted the arcade's BAE2..BB29 state transition. The
  terrain-specific turn, hop or floor-change action now runs, including the
  turn fallback when a floor change is refused. Spawn positions are preserved.
  A fixture using the authored X816/Y232 spawn inside the column escapes to
  X608; a second fixture verifies the response at the column's edge.

These four emulator fixtures fail on released v0.9 and pass on the candidate.
They seed positions/actor state near authored terrain, then run the production
loop; they do not establish whole-game or physical-hardware coverage.

## Reports already addressed or clarified

- [rollingthunder4's Alpha-era BULLETS-door report](https://itch.io/post/17659016):
  tested the exact Stage 1 door at X2016/Y65 before the basement stairs. It
  gives 50 primary rounds, also when a machine gun is equipped. Cancel/retry
  works and an already consumed door gives no second refill. Three Home SRAM
  slots and area-start continuation already address the accessibility request.
  The comment about more forgiving collisions did not specify a failing
  contact; no speculative hitbox or difficulty changes were made.
- [cgamebit's retest](https://itch.io/post/17658398) confirms v0.9 works on their
  Genesis Model 1 VA7 with an EverDrive V2 OS3.6 clone, and their V3 Pro.
  This resolves that tester's v0.81 boot report, not every cartridge/console
  combination. A separate expansion-bus suggestion is an unverified hardware
  hypothesis, not evidence for a software workaround.
- [Jankins97's devlog report](https://itch.io/post/17654392): distinct save-slot
  labels, title animation cadence and automatic Arcade attract entry were
  already fixed in the local v0.91 candidate and pass the regression suite.
- [Captain Past's graphics report](https://itch.io/post/17656677): the prior
  palette-transfer and sound-test loading fixes remain included. Console
  retesting is still needed. Leila's restored hair colors remain as requested;
  broad cave/door hue preferences have not prompted a shared-palette change.

## Audio remains open

[Captain Past's clarification](https://itch.io/post/17658260) was already
received and investigated. The rushed opening rhythm is fixed locally. Track 7
headroom, later Main BGM 1 stalls, instrument/percussion fidelity and the faint
high tone are **not** being marked resolved by these gameplay changes. See
[V0_91_AUDIO_FEEDBACK.md](V0_91_AUDIO_FEEDBACK.md) for the recording and measured
limits. No new audio changes were made in this feedback pass.

## Validation and candidate

- 12,288 native projectile material cases cover all flag bytes, four layers,
  both directions, player/enemy shots and all three production entry paths.
- 8,192 real wall-sweep/entry cases cover both walking states/directions,
  terrain codes, variations and accepted/refused floor changes.
- The source arcade ROM signatures independently verify the bullet mask and
  wall-response selector table. No arcade program instructions are embedded
  in the cartridge.
- All ten door atlases are checked; the live VDP test verifies 2,048 opaque
  black pixels inside the opening door while retaining the green backdrop.
- The 16 feature regression groups and 22 timing/menu/pacing checks pass.
  Solo's 900-update Stage 2 workload remains 952 refreshes / 900 presentations.
  Co-op records 929 refreshes / 742 presentations versus v0.9's 928 / 741;
  full/reused presentations are 489/253 versus 491/250. The corrected bullet
  behavior changes the workload. These are limited deterministic emulator
  measurements, not a whole-campaign performance guarantee.
- A normal full build matches the tested runtime candidate byte for byte.
  The ROM is **4,194,304 bytes / 32 Mbit**; no game content was removed.

Local file: `dist/Rolling_Thunder_MD_v0.91_Beta_Candidate.bin`.
SHA-256: `15bbd465776b3b875896ebafd06166bdc3f9fa161977658a931949d2d7515675`.
Reports: [validation/v0.91](validation/v0.91), particularly `board-*.json`.
