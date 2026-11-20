# v0.92 stage feedback — Area 5 lasers, Area 7 restart points

Reports from rollingthunder4 (v0.92, EverDrive Pro / Analogue SG):

1. "In Area 5, there are no lasers coming from the ceiling."
2. "In Area 7, there are no save points. It seems virtually impossible to
   complete the level in the time allotted."

U.S. PLATINUM INTERACTIVE DEVELOPMENT also asked for Home-mode mid-stage
checkpoints. Only the part that overlaps report 2 is addressed here.

## Area 5 ceiling lasers — not present in the arcade

Evidence from the owner-supplied revision 3 `rthunder.zip`:

- **Actor inventory.** The CPU2 actor grid for story 1 area 5 (native
  `enemySpawns{0,1}_0_4`) contains only families 4, 8 and 12: masked guards and
  the blue guards that drop from the ceiling in the final control room
  (templates 191/195/196). No high-object family (32–44) or Type 10 actor.
- **Stage objects.** The area's event table (`events0_4`) contains only doors
  (kind 1), climb anchors (kind 3) and decorations (kinds 9/10). The port
  services all of these; the object pass reports no unserviced kinds there.
- **Arcade playback.** MAME `rthunder` with the Level Select and
  Invulnerability DIPs, scripted to walk from the Area 5 entrance into the
  Story 1 clear sequence. Snapshots every 15 frames through the second half and
  every 6 frames through the control room show grenades, guards and the ceiling
  drop-in guards, but no laser emitter or beam. Story 2's Area 5 (Area 10),
  loaded through the same level-select path, likewise showed none in the
  covered section.
- **Port comparison.** A GX diagnostic entering native Area 5 at X 3000 shows
  the same blue ceiling guards dropping into the control room.

No laser hazard was added: there is no arcade behavior to reproduce. If the
reporter can name the spot (or share a clip), it may be the ceiling guards or a
grenade burst on the upper walkway near the ladder.

## Area 7 restart points and timer

- **Restart point.** CPU1 `$A592` restarts a life at area-header word `$7A`
  (entry) unless DP `$CF` is set, when it uses word `$7C` (mid-area restart).
  Header bytes from the bank-3 area table:

  | Area | story/area | `$7A` | `$7C` |
  | --- | --- | --- | --- |
  | 1 | 0/0 | 0001 | 0801 |
  | 2 | 0/1 | 0002 | 0703 |
  | 3 | 0/2 | 0000 | 0800 |
  | 4 | 0/3 | 0001 | 0800 |
  | 5 | 0/4 | 0000 | 0000 |
  | 6 | 1/0 | 0001 | 0801 |
  | **7** | **1/1** | **0002** | **0002** |
  | 8 | 1/2 | 0001 | 0E01 |
  | 9 | 1/3 | 0005 | 0005 |
  | 10 | 1/4 | 0000 | 0400 |

  Area 7 has no mid-area restart point in the arcade (it differs from Area 2,
  whose map it shares). The port's `rtWorldStages[6].entrances` match.
- **Timer.** The only timer source is the two-entry BCD table `0120 0150` at
  CPU1 `$9CB2` (and its copies), selected by the Timer DIP. There is no
  per-area value. The port's 120/150 matches; Area 7 is not short-timed.

### Home Campaign now uses the arcade restart points

Arcade mode already honored these points. Home Campaign did not: every paid
continue rebuilt the area at its entrance, so Areas 1, 2, 3, 4, 6, 8 and 10 lost
their arcade mid-area restarts. Home now tracks the same restart window as
Arcade (`RT_arcadeObserveWindow`) and a paid continue in that area restarts at
`$7C` once it has been reached. Timer and enemies restart as before. Survival
and co-op survivor respawns are unchanged. Area 7 still restarts at its
entrance, as in the arcade. No new, non-arcade checkpoints were invented.

## Tests

`tests/test_home_restart_point.py` (GX):

- `reached`: Area 1 entered at camera X1888, walked right past the X2048
  restart window, then a lethal state is seeded. v0.92 restarts at X0 (fails);
  the fix restarts at X2048.
- `not-reached`: Area 1, short walk from the entrance. Restarts at X0 on both.
- `area7`: Area 7 restarts at its entrance; asserts the authored restart equals
  the entry.
