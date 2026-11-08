# Local menu colors and Arcade achievements

Candidate: `dist/menu-arcade-candidate/RollingThunderMD.bin`.
SHA-256: `41eb83ddbd2eaeccd2a6641a273f79713ed6c9e58062f97696899ef673805c1c`.
Size: 4,194,304 bytes / 32 Mbit. Used span: 4,189,406 bytes.
No publication or upload. The versioned v0.81 ROM remains unchanged.

The menu captures show the actual emulator output for Main, Home, Arcade and
Achievements. White/light-gray text and its dark shadow use the console's
existing palette, preserving the backdrop, cast and six text sprites.

- `logic.txt`: all 21 Arcade goals, single unlock cue, persistence, exclusion
  of Home/Survival/demo/cheats, default DIP rules, spare lives, shot-count reset,
  continue failures, and migration from old Home medal records. Three Home
  checkpoints and their inventory remain valid.
- `sram-gx.json`, `sram-pico.json`: controller-driven three-slot saves and fresh
  emulator-process SRAM restoration; Arcade awards persist, Home does not
  award them, and the transient Arcade banner clears after its timer expires.
- `journals.txt`: interrupted writes, corruption fallback, isolated journals,
  and warm-reset SRAM restoration.
- `hud.txt`: invalidating Arcade restores its blank banner cells while Home
  retains CREDIT and independent pistol/machine-gun counters.
- `frontend.json`: both controllers' mappings, character-selection redraws,
  and 30 menu setting commits across the background's palette split.
- `arcade-options.json`: all six switches, their gameplay consumers and reset.
- `attract.json`: two complete attract loops, script checks, Start/Back in each
  phase, and unchanged SRAM/records during demos.

The paired Area 2 co-op sample used 900 simulation updates and the same initial
refresh phase and input stream. The preceding Home-feature build needed 925
play refreshes; this revision needed 924. Both presented 743 frames. Solo Area 2
used 900 play refreshes and 898 presentations. These bounded emulated workloads
show no slowdown in the tested cases; they are not full-game hardware benchmarks.
`baseline-stage2-2p.json` intentionally identifies the prior ROM hash.

Unlocks are local cartridge achievements based on the arcade RetroAchievements
set, not online RetroAchievements account integration. The first 17 goals require
default gameplay DIPs; demo sound may be muted. Invulnerability excludes all goals.
Physical-console and flash-cartridge testing has not been performed for this revision.
