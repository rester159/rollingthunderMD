# Rolling Thunder MD v0.81 Alpha

[Quick dev log](DEVLOG_0_81.md)

Released source snapshot: October 18, 2026. Local ROM:
`dist/Rolling_Thunder_MD_v0.81_Alpha.bin`.

- Exactly 4,194,304 bytes (4 MiB / 32 Mbit), linear cartridge layout.
- Includes both current portraits, verified byte-for-byte in the ROM and in an
  [emulator capture](releases/0.81/portraits-in-rom.png).
- Includes Arcade attract, frontend/controller improvements, player visibility
  inside doors, sound-menu recovery, transition audio, and PAL startup fixes.
- Game-over/restart and frontend tests pass on Genesis Plus GX and PicoDrive;
  nine regional boot cases and the additional door/audio/scene checks pass.

SHA-256: `54eba2215c07860b6644426bf82d97fcbea3070dbe893e0edfab3fbedca9156d`.
The versioned ROM is byte-identical to the tested feedback candidate; no rebuild
or binary patch was made while assigning this version.

## Known limitations

The [24-run paired benchmark](releases/0.81/benchmark.md) does not certify the
release regression-free. Later-stage solo Leila samples lose roughly 1.5–2
simulation updates per second. Final-stage co-op displays roughly 38 changed
frames per second in both baseline and candidate. Frame skipping remains enabled.
No full-game playthrough, physical EverDrive, KFusion or Mednafen certification.

See [feedback fix details and test scope](ITCH_FEEDBACK_FIXES.md). ROMs and
owner-supplied audio remain local, following the repository's existing policy.

The release source is taken from the isolated tested build where newer shared
workspace edits differ; those newer edits are not part of v0.81.
