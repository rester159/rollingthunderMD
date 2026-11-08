# Feedback candidate benchmark — 2026-10-18

Result: not certified regression-free. Solo Leila samples in stages 5 and 10 have lower simulation and presentation rates. Late-game co-op remains below 50 displayed FPS in both builds.

Candidate SHA-256: `54eba2215c07860b6644426bf82d97fcbea3070dbe893e0edfab3fbedca9156d`.
Baseline: published v0.8, SHA-256 `e30b81671e1277b2ad586be717fb4420085d9971e37813c97a4cef2668b7e7ed`.

24 headless runs: 12 paired scenarios, each 1,800 NTSC refreshes (about 30 seconds). Opening runs start at simulation update 400. Later-stage runs enter through the production stage loader with refresh decision phase zero. Movement and shooting inputs follow simulation updates. Rates below exclude refreshes entering/leaving non-gameplay states. No ROM changed during measurement.

Scenario names use zero-based stage indices: stage4 is stage 5, stage9 is stage 10.

| Scenario | Updates/s old → new | Presentations/s old → new | Changed video FPS old → new |
|---|---:|---:|---:|
| genesis_plus_gx-campaign_1p-agent0 | 59.82 → 59.65 | 59.72 → 59.54 | 50.38 → 50.33 |
| genesis_plus_gx-campaign_1p-agent1 | 59.82 → 59.58 | 59.72 → 59.47 | 50.38 → 50.44 |
| genesis_plus_gx-campaign_2p-agent0 | 59.89 → 59.85 | 59.79 → 59.75 | 53.64 → 53.61 |
| genesis_plus_gx-stage4-campaign_1p-agent1 | 59.32 → 57.27 | 59.10 → 57.09 | 52.08 → 51.49 |
| genesis_plus_gx-stage4-campaign_2p-agent0 | 59.65 → 59.65 | 57.86 → 57.59 | 55.49 → 55.22 |
| genesis_plus_gx-stage9-campaign_1p-agent1 | 54.29 → 52.74 | 54.07 → 52.51 | 51.74 → 51.18 |
| genesis_plus_gx-stage9-campaign_2p-agent0 | 55.20 → 55.39 | 38.83 → 38.96 | 37.90 → 38.17 |
| genesis_plus_gx-survival_2p-agent0 | 59.49 → 59.45 | 55.21 → 55.65 | 51.36 → 51.80 |
| picodrive-campaign_1p-agent0 | 59.82 → 59.54 | 59.82 → 59.54 | 50.38 → 50.35 |
| picodrive-campaign_1p-agent1 | 59.82 → 59.61 | 59.82 → 59.61 | 50.38 → 50.44 |
| picodrive-campaign_2p-agent0 | 59.82 → 59.82 | 59.85 → 59.82 | 53.61 → 53.58 |
| picodrive-survival_2p-agent0 | 59.75 → 59.66 | 52.76 → 58.58 | 46.79 → 53.19 |

## Interpretation and limits

Updates measure gameplay simulation. Presentations count committed rendering updates, including reuse/reprojection. Changed video FPS counts consecutive framebuffer differences: stationary/identical images do not count, while animated HUD or raster changes can count. These metrics are deliberately reported separately. Frame skipping remains enabled as configured in each ROM.

The same input policy does not guarantee identical world states after timing differences; samples include death/respawn paths and have slightly different active-play durations. These results flag a performance regression for investigation, rather than identifying its cause. They are not a full-game performance guarantee or physical-console measurement. No runtime fault or compressed-graphics staging exhaustion occurred in the 24 runs.

## Portrait confirmation

All current portrait tile, map and palette bytes were found exactly in the delivered ROM. `portraits.json` records offsets and hashes. `portraits-in-rom.png` is an actual headless emulator capture of the selection screen, showing both updated portraits. No ROM rebuild or artwork modification was performed for this benchmark.
