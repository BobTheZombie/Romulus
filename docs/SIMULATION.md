# Simulation architecture

`romulus::sim` is the deterministic, SDL-independent city simulation layer. Rendering,
input, audio, and original-data compatibility must consume simulation state rather than
owning game rules themselves.

## Current foundation

The first simulation slice provides:

- a rectangular tile map with terrain, zoning, roads, and building occupancy;
- deterministic four-neighbour road routing with A* and Manhattan distance;
- player-facing road construction, zoning, bulldozing, and scenario terrain setup;
- deterministic zone growth into houses, shops, and workshops;
- population, jobs, unemployment, demand, treasury, taxes, and maintenance;
- fixed simulation ticks with configurable growth and economy intervals; and
- a stable state hash for regression tests and deterministic replay verification.

The simulation intentionally has no SDL dependency. Tests can advance thousands of
simulation ticks headlessly, and the renderer can later interpolate or display snapshots
without changing authoritative state.

## Determinism rules

- Tile scans are row-major.
- Road neighbours use a fixed north/east/south/west order.
- A* ties are resolved by total score, heuristic, row, then column.
- Zone growth develops at most one building per growth interval.
- Building IDs increase monotonically from 1.
- Economy processing happens only on configured economy tick boundaries.

Any future random behaviour should use an explicit simulation-owned PRNG with a saved
seed/state. Do not use wall-clock time or platform randomness in gameplay systems.

## Next simulation milestones

1. Multi-tile building footprints and building definitions loaded from clean-room data.
2. Road-network components, entrances, walkers, destinations, and service dispatch.
3. Water, fire risk, damage, health, crime, desirability, and service coverage layers.
4. Housing evolution and devolution driven by service/desirability requirements.
5. Warehouses, markets, industry chains, resource production, imports, and exports.
6. Employment assignment, migration, wages, taxation policy, and richer demand models.
7. Scenario objectives, events, military/province systems, disasters, and campaign state.
8. Versioned save/load and deterministic replay files.

Caesar II behaviour should continue to be studied clean-room. Original proprietary
assets or source code must not be committed to Romulus.
