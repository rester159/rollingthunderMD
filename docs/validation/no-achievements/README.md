# Local build with achievements removed

October 19, 2026. Candidate: `dist/no-achievements-candidate/RollingThunderMD.bin`.
SHA-256: `318691b01538e9165fa64d29302ec27731e916006a13997bedf76835bc61baf4`.
Size: 4,194,304 bytes (32 Mbit). No publication or release.

Removed the achievement menu, condition tracking, per-frame hooks, notification,
unlock sound requests and saved unlock handling. The separate RA prototype,
exporter, test and downloaded development dependency were also removed.
The ROM contains no achievement symbols or menu/notification text.
Normal game sounds, scores and extra lives are retained.

Home's three SRAM slots and title Continue, Arcade options/DIPs, attract mode,
transparent HUD and the recent menu recoloring remain. Both prior profile
versions still load their DIP settings and selected save slot. Reserved legacy
profile bytes are ignored and written as zero on the next ordinary profile save;
checkpoint records keep their original layout.

Validation:

- Complete normal build, all ten frozen stages reproduced, checksum valid.
- Host tests: three checkpoints, dead co-op partner handling, prior profile
  compatibility, SRAM journals/interrupted writes and Home HUD ammo/credits.
- Genesis Plus GX and PicoDrive: three slots, fresh-process SRAM restore,
  persisted options and a three-entry Arcade menu with correct wrapping.
- Controller mapping/selection regression, all six DIP settings and defaults,
  and two complete attract loops with unchanged SRAM during demos.
- Area 2 samples: solo 900 play refreshes / 898 presentations; co-op 925 play
  refreshes / 743 presentations over 900 simulation updates. The preceding
  menu candidate measured 924 / 743 for co-op: one refresh difference, with
  the same presentation count. These are bounded emulator workloads.

The versioned v0.81 ROM and CURRENT_BUILD.txt remain unchanged. Older validation
folders describe their historical builds. No physical hardware test was run.
