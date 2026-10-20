# Solo Home ammo-door follow-up — unreleased

The user clarified that the Stage 2 door sometimes failed to refill ammo in
**solo Home**, and could not recall whether the machine gun was equipped or the
door had already been used. The earlier co-op race fixes do not explain that
report.

## Reproduced display defect

Entering the actual Area 2 primary supply door with 50 pistol rounds and 80
machine-gun rounds correctly increases pistol ammo to 70. The previous Home HUD
continues displaying 80, because it substitutes the equipped machine gun's count
for the pistol count. The refill therefore appears to do nothing.

The original arcade displays the two counters separately. Its CPU1 routine at
$9360 conditionally prints the machine-gun count, then always prints the pistol
count. CPU2's refill routine at $9F4C selects the inventory from the door type,
not the equipped weapon. The reference ROM hashes and routine locations are in
[arcade-reference.json](validation/solo-ammo/arcade-reference.json).

Home now keeps pistol rounds in its existing bottom counter and shows a separate
`MG` counter alongside the credits when a machine gun is equipped. Each player
has an independent MG counter in co-op. The extra field clears when that weapon
is no longer equipped. This uses the existing HUD rows and glyphs; weapon
selection, ammo quantities, refill timing and the playfield are unchanged.
Arcade's existing HUD path is outside this Home-specific display change.

This corrects a reproducible way to miss a successful refill. It does not prove
that every reported failed visit had this cause; a pistol-only first visit that
still fails would need a separate reproduction.

## Tests

[Evidence folder](validation/solo-ammo/).

- Actual Area 2 supply objects loaded by the production loader, entered and
  exited using Up input. The test relocates the emulator's stage entrance and
  seeds player position/inventory/protection; it does not inject door flags,
  player actions, animation phases or refill timers.
- PAL and NTSC, Albatross and Leila: ordinary pistol refill (50→70), pistol refill
  while carrying 80 machine-gun rounds (50→70 and MG remains 80), interrupted
  entry followed by a successful retry, and MG pickup (0→80 while pistol remains
  50). Returning to each consumed door gives no additional supplies.
- The previous candidate fails the new counter-visibility assertion. The new
  ROM shows both inventories correctly. A native screenshot is included.
- Independent co-op counter updates/clearing, credit preservation and dirty-row
  behavior pass a host test. Earlier co-op refill fixes pass in PAL and NTSC.
- Both emulator frontend suites, regional boot/pause/reset checks and both
  characters' door-visibility tests pass.
- Matched 900-update Campaign samples: solo Areas 1/2 retain exactly the same
  refresh and presentation counts. Co-op Area 1 has the same refresh count and
  one fewer presentation; Area 2 takes one more refresh and has one more
  presentation. These small sample differences do not establish whole-game or
  physical-hardware performance. See `timing-summary.json`.

## Local artifact

`dist/solo-ammo-candidate/RollingThunderMD.bin`

SHA-256: `ba3f09424c140066af50b29e90a01ab5aaa1a263a359658bbfdf9b176df59d12`.

4,194,304 bytes (**4 MiB / 32 Mbit**), with payload end 4,189,406 and 4,898 bytes
spare. Includes the preceding black HUD backing and co-op door fixes, existing
lossless optimizations and accepted portraits.

No release, push or upload. The released v0.81 ROM and `CURRENT_BUILD.txt` remain
unchanged. This build has not been tested on the reported physical clone.
