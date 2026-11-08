# Leila sprite lipstick removal

October 19, 2026. Local candidate:
`dist/leila-no-lipstick-candidate/RollingThunderMD.bin`.
SHA-256: `b1c2b46e5596950d76e9bd4f2dffcae2c17b8135110422fd11c1fc46abd0d598`.
Size: 4,194,304 bytes (32 Mbit). No release or upload.

Reviewed all 73 populated Leila poses, both costumes, in all ten stage banks.
Replaced the lipstick with existing skin/shadow colors wherever present:
29 poses per costume, 33 pixels per costume per bank (660 pixel substitutions
across all banks). Other poses already have neutral mouths or hide the face.
The left-facing versions share these patterns and use the hardware flip.
The blue costume's corresponding blue lipstick was removed too.

`tools/remove_leila_lipstick.py` records the reviewed mouth coordinates and
validates source colors per stage palette. Re-running it makes zero changes.
Only Leila pattern references and the corresponding hashed pattern definitions
were added/changed. All other sprite pixels, Albatross variants, shared palettes,
sprite geometry, animations, portraits and runtime code are unchanged by this
edit. The frozen bank hashes were regenerated with the normal compiler/linker;
normal build validation remains enabled.

Validation:

- Compared all 1,460 populated Leila pose entries with the preceding tracked
  artwork: exactly the expected mouth pixels differ; all other pixels and
  geometry remain exact. The twelve null pose entries per bank are unchanged.
- Full normal build reproduced all ten updated frozen stage banks and passed
  packed graphics, audio and final cartridge checks.
- Genesis Plus GX controller-driven solo campaign, two-player campaign and
  two-player survival checks passed with Leila, including movement, shooting
  and jumping (240 updates each; no runtime faults).
- Deterministic Area 2 two-player sample: both builds produced 741 presentations
  over 900 simulation updates. Baseline took 927 refreshes, new build 928;
  no graphics allocation failure. This is a bounded emulator sample, not a
  full campaign or physical hardware measurement.
- `comparison.png` shows red/blue costume profile and frontal mouth changes.

The published versioned v0.81 ROM and CURRENT_BUILD.txt were not changed.
