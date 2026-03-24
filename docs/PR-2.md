# PR 2 - Deterministic-friendly app loop and timing layer

## Scope

This PR introduces a fixed-timestep update loop and moves SDL runtime ownership
into a dedicated platform application class while preserving the current smoke-test
execution path.

## Changes

- Added `romulus::core::FixedTimestepClock` to manage a fixed-step accumulator with:
  - clamped maximum frame delta (`250ms`)
  - negative frame delta protection
  - interpolation alpha output for rendering
- Added `romulus::platform::Application` as the runtime owner for:
  - SDL init/shutdown
  - window creation/destruction
  - event processing
  - fixed timestep loop execution
- Slimmed `apps/caesar2/main.cpp` to argument parsing + application launch.
- Added timing unit coverage in `tests/core/timestep_tests.cpp`.
- Updated CMake wiring for platform module and test target.

## Notes

- Renderer and simulation remain explicit stubs (`render_frame` / `simulate_step`).
- Smoke-test behavior remains available via `--smoke-test`.
- This PR intentionally avoids unrelated refactors.
