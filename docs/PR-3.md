# PR 3 - Data root filesystem validation

## Scope

This PR adds a narrowly scoped filesystem/data-root layer for locating and validating
user-supplied Caesar II data before the SDL runtime starts.

## Changes

- Added `romulus::data` module with:
  - data-root path resolution (`resolve_data_root`)
  - required file/layout declarations
  - root validation (`validate_data_root`)
  - user-facing error formatting (`format_validation_error`)
- Added command-line parsing in `apps/caesar2/main.cpp` for:
  - `--data-dir <path>` to choose a candidate data root
  - existing `--smoke-test` behavior
  - clear usage and argument error output
- Integrated early data-root validation into `romulus::platform::Application` so
  invalid roots fail cleanly before SDL initialization.
- Added tests for:
  - relative path normalization
  - nonexistent root error reporting
  - missing required-entry reporting
  - successful validation when expected layout is present

## Notes

- This PR intentionally does **not** parse game assets yet.
- Renderer and simulation remain unchanged.
- Required data entries are a small initial compatibility baseline and can be
  expanded in later PRs.
