# Leila purple hair, shared palettes preserved (reverted)

October 19, 2026. Local development candidate:
`dist/leila-purple-candidate/RollingThunderMD.bin`.
SHA-256: `598cfa346d9e80277de9aff51346ffd2c76a8650cd1490b2c9c2d0a11a31cef6`.
Size: 4,194,304 bytes (32 Mbit); full normal build and header checksum passed.

The user selected blended purple shading while keeping enemy colors unchanged.

The authored hair mask covers all 73 populated poses for each Leila costume in
all ten stages (1,460 pose entries). Both facings share the changed patterns.
Existing pink, dark blue and plum pens form the purple approximation; there is
no palette replacement, extra sprite, runtime recoloring, or rendering pass.
This is the approved palette-safe approximation, not an exact reproduction of
the portrait's private purple palette.

`assets/ui/leila/gameplay-hair.json` stores each selected hair pixel and its
existing Genesis color. `tools/recolor_leila.py` resolves these colors into each
stage's palette, validates every stage before writing, and is idempotent.
Original shared pattern arrays remain immutable; changed patterns have new
content-hashed identities. Geometry, tile counts and pose scheduling are exact.
The prior lipstick removal is retained.

Artwork validation compared against the preceding lipstick-removal candidate:

- All 1,460 populated Leila poses match the authored hair edits.
- Non-hair pixels and all recorded neutral mouth pixels remain unchanged.
- Albatross pose references, geometry and all non-agent stage sources are exact.
- Shared palette data, enemy art and scenery are byte-for-byte unchanged.
- Reapplying the hair or lipstick tools changes zero pixels.

`comparison.png` shows before/after samples from the red and blue costumes.
Build and emulator results are recorded in the adjacent JSON reports.
The versioned v0.81 release and CURRENT_BUILD.txt are not changed or published.

Emulator validation: Genesis Plus GX passed solo campaign, two-player campaign
and two-player survival with Leila (240 movement/shooting/jumping updates each,
no runtime faults). In the deterministic Area 2 two-player sample, 900 updates
took 927 refreshes with 742 presentations, versus 928 refreshes and 741
presentations in the preceding candidate. No slowdown appeared in this bounded
sample; this is not a full campaign or physical-hardware benchmark.

Reverted at the user’s request on October 19, 2026. The active source and
local candidate again use the preceding hair colors, with lipstick removed.
Current candidate: `dist/leila-no-lipstick-candidate/RollingThunderMD.bin`.
