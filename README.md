# Romulus

Romulus is a clean-room, open-source reimplementation of **Caesar II** for Linux.
Its initial goal is to provide a small, modern C++20 codebase that can grow into a
faithful reimplementation without relying on proprietary game source code or bundled
commercial assets.

## Clean-room and legal boundary

This repository does **not** include any original Caesar II source code, art,
audio, or data files. Development should remain clean-room: behavior may be
studied and documented, but implementation must be written from scratch.
Support for the original game will eventually rely on **user-supplied** data from
legitimately owned copies.

## Current project structure

The codebase is organized for future modules:

- `apps/caesar2/` - executable entry point
- `src/core/` - shared utilities such as logging
- `src/platform/` - Linux platform abstraction scaffolding
- `src/render/` - rendering systems scaffolding
- `src/data/` - data loading and asset pipeline scaffolding
- `src/sim/` - simulation scaffolding
- `src/ui/` - user interface scaffolding

## Build instructions

### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libsdl2-dev
cmake -S . -B build
cmake --build build
```

### Run

```bash
./build/caesar2
```

The application currently opens an SDL2 window titled `Caesar II Reimplementation`
and exits cleanly when the window is closed. A `--smoke-test` flag is also available
for short automated launch checks in headless environments.

## Roadmap note

Future milestones will focus on platform services, rendering, simulation systems,
and support for loading **user-supplied original game data** without redistributing
proprietary assets.
