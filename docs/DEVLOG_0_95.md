# Rolling Thunder MD v0.95 Beta

The v0.95 download is `Rolling_Thunder_MD_v0.95_Beta.bin`, still a 4 MiB /
32 Mbit cartridge image.

This release brings the v0.94 sound fix onto the project's `main` branch and
updates the version shown in the game. Gameplay and sound behavior are the same
as v0.94. The fix keeps track of who paused the Z80 music driver and clears a
pause nobody owns, addressing reports of silence on some consoles.

The new ROM passed the audio-guard, watchdog, and reset checks in Genesis Plus
GX. If you saw the silent-console issue, a cold-start test on the affected
hardware is still valuable; please include your console and cartridge model.
