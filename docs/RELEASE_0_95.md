# Rolling Thunder MD v0.95 Beta

Source snapshot: November 27, 2026.
ROM: `dist/Rolling_Thunder_MD_v0.95_Beta.bin`.
Matching symbols and build metadata: `dist/v0.95-beta/`.

Size: **4,194,304 bytes (4 MiB / 32 Mbit)**.
SHA-256: `a97d6bea955515dd7cf3085b751fa860c9794f5ebe5a5d772effe031e2f3f90f`.

The v0.94 silent-console fix is now on `main`. This v0.95 build changes the
on-screen version label; there are no new gameplay or audio changes since
v0.94. The rebuilt v0.95 and v0.94 ROMs differ at two byte positions.

## Validation on the v0.95 ROM

- Rebuilt from the owner-supplied arcade archive with the pinned ten stage
  banks and four finite PCM clips reproduced. The compact ROM and audio
  placement checks passed.
- Genesis Plus GX: the music watchdog passed healthy, stale PROTECT, and halted
  driver cases. The stale flag cleared without a driver reload.
- Genesis Plus GX: three nested DMA calls preserved the outer Z80 reservation;
  an un-nested DMA released its own reservation.
- Genesis Plus GX: six repeated soft resets passed in each of Arcade,
  one- and two-player Campaign, and one- and two-player Survival.

The exact console interrupt race is not reproduced by these emulator tests.
The reports are under `docs/validation/v0.95/`; physical-hardware confirmation
on affected consoles remains pending.
