# Arcade attract mode — local, unreleased

Selecting Arcade now opens its attract loop. Start (or A) begins a fresh normal
Arcade game from any attract phase. B returns to the Arcade/Home selector. Home
Campaign and Survival retain their existing entry flow. Reset returns to the
initial mode-selection screen. The port credit and v0.81 stay on that screen.

The loop uses the native renderer for four phases:

1. Animated title and console cast, with a Start prompt.
2. Area 1 demonstration using the arcade's 38 action selectors from CPU2 EB00.
3. The saved TOP 5 table, displayed without entering or modifying scores.
4. The animated Maboo portrait, followed by the title again.

The visible title, score-table and portrait holds follow the reference's
512/256/192 refresh counts, scaled for PAL. Demonstration actions advance when
the current native action completes: walks, standing/kneeling fire, vaults,
drops, and the supply-door visit. Normal native physics, collision, enemies and
rendering remain active. Death ends the demo; a 1,536-update limit also ensures
the loop returns if the native encounter outcome differs from the reference.
This is a translation of the original action script, not a frame-perfect video
replay or a claim of new full-game arcade timing parity.

The attract flag prevents score awards, credit use and save writes. Start ends
all attract video/audio owners and initializes a fresh session; demo position,
ammunition, health and actor state do not carry into play. Controller events are
buffered through scene loading. Holding Start across the transition does not
pause the new game. Completing Arcade's score-entry flow returns to attract.

## Evidence and validation

The owner-supplied revision 3 arcade ZIP was observed headlessly in FBNeo. The
reference cycles title → demo → TOP 5 → Maboo; its script bytes and first walk/
fire action durations are checked by `tests/test_attract.py`. Local reference
captures and CPU-state traces are under `.local/attract-reference/` and excluded
from Git. Screenshots are emulator output, not generated illustrations.

The local ROM and matching reports are in `dist/attract-candidate/`. This folder
is not a release. The published v0.8 ROM and the prior comments-fix candidate
remain unchanged. No upload or Git push is included.

The source cleanup removes the unreachable old static-menu fallback. Its
unused title bitmap no longer occupies the fixed-ROM area, leaving room for
attract code without removing any active game content.

Final local candidate SHA-256:
`caee92199d740d37b66a3867e0293abdc48c946e8a44511cf3bb8e5803254d0a`.
Both Genesis Plus GX and PicoDrive passed two complete loops, all 38 action
selectors, Start/Back from every phase (including a brief press at demo loading),
held-Start handoff, save isolation, Home inactivity and reset checks. All 15
broader regression jobs also passed. The host save suite, source audit and
whitespace checks passed. The contact sheet in the candidate folder was inspected.

Campaign opening samples measured 58.39 updates/presentations per second in 1P
and 58.52 in 2P, with the same 21/20-refresh longest reloads as the accepted
build. Survival measured 49.97 updates and 46.31 presentations per second.
These are emulator opening samples, not physical-hardware or full-game results.

## Corrected cartridge capacity target

The October 18 target is **32 Mbit = 4 MiB = 4,194,304 bytes**, superseding the
earlier 7 MiB target. This attract candidate is still **7,340,032 bytes = 56 Mbit**,
so it exceeds the target by 3,145,728 bytes. Attract validation does not qualify
it for a 32 Mbit cartridge. A smaller playable ROM still needs to be implemented
and validated without cutting game content. Everything remains local/unreleased.
