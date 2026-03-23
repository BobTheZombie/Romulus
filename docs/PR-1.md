# PR 1 Design Notes

## Goals

PR 1 establishes the smallest viable Linux-first project skeleton for a clean-room
Caesar II reimplementation. The emphasis is on a buildable foundation, a visible SDL2
application entry point, and simple shared infrastructure that later modules can reuse.

## Key design choices

### CMake-first Linux build

- Uses a single top-level `CMakeLists.txt` with C++20 enabled.
- Depends on system SDL2 through `find_package(SDL2 REQUIRED)`.
- Keeps the initial executable wiring straightforward so the project is easy to build
  locally and in CI.

### Simple modular layout

The repository is organized around future subsystems from day one:

- `core`
- `platform`
- `render`
- `data`
- `sim`
- `ui`

Only `core` contains implementation code in PR 1. The remaining module directories are
present as scaffolding so future pull requests can expand the architecture without a
large structural refactor.

### Minimal logging utility

A tiny logging utility in `src/core` provides:

- a small `LogLevel` enum
- a single formatted logging function
- convenience wrappers for info, warning, and error messages

This is intentionally lightweight and avoids introducing third-party logging
infrastructure before the project needs it.

### SDL2 bootstrap application

`apps/caesar2/main.cpp` initializes SDL video, opens a window titled
`Caesar II Reimplementation`, processes the quit event loop, and shuts down cleanly.
That gives the project a verifiable runtime entry point while keeping the diff small.

### Clean-room boundary

Documentation explicitly states that this repository does not ship proprietary assets
or original source code. Future compatibility is expected to rely on user-supplied
original game data.
