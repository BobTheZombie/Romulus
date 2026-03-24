# PR 9 - Narrow Debug Image Export Path

## Scope
This PR adds a small, tool-oriented path to export one decoded Caesar II indexed image as a visible file for debugging. It does **not** add rendering systems, resource managers, atlases, or simulation behavior.

## What was added

### 1) RGBA export utility (PPM)
Added `export_rgba_image_as_ppm` in `romulus::data`:
- input: `RgbaImage`
- output: binary PPM (`P6`) file
- validates:
  - non-zero dimensions
  - exact RGBA byte count (`width * height * 4`)
- writes RGB channels from RGBA (alpha is intentionally ignored by PPM)
- returns structured export errors for invalid image data or I/O failures

PPM was chosen intentionally as the simplest first debug output format.

### 2) CLI path to decode + export one image
Added a narrow CLI flow in `apps/caesar2`:
- `--export-tile-file <path>`
- `--export-palette-file <path>`
- `--export-output <path>`
- optional `--index-zero-transparent`

Behavior:
1. load tile file bytes
2. load palette bytes
3. parse tile using existing strict indexed tile parser
4. parse palette using existing bounded palette parser
5. convert indexed image -> RGBA via existing palette application helper
6. export resulting RGBA image as PPM

Relative paths are resolved against the data root (`--data-dir` behavior is preserved).

### 3) Tests
Added `romulus_image_export_tests` covering:
- successful PPM export with header and RGB payload checks
- malformed RGBA input rejection (buffer-size mismatch)

## Outcome
A supported Caesar II indexed image can now be decoded and exported to a visible image artifact using a minimal debug-first tool path, while maintaining strict parsing and clean failure behavior.
