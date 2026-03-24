# PR 8 - Simple Indexed Tile Decode (Caesar II)

## Scope
This PR adds one narrowly scoped graphics decoder for a simple Caesar II indexed tile resource. It does **not** introduce sprite systems, atlases, animation orchestration, or rendering architecture.

## Resource model
Added `IndexedImageResource`:
- `width` (`u16`)
- `height` (`u16`)
- `indexed_pixels` (`width * height` bytes, 8-bit palette indices)

Added optional `RgbaImage` output model for debug-friendly conversion after decode.

## Supported format (single format only)
`parse_caesar2_simple_indexed_tile` currently supports one simple binary layout:
1. `u16` little-endian width
2. `u16` little-endian height
3. `width * height` bytes of indexed pixels

Validation is strict:
- dimensions must be non-zero
- dimensions are bounded (`<= 1024` each)
- total pixels are bounded (`<= 1024 * 1024`)
- payload must contain exactly `width * height` pixel bytes
- trailing bytes are rejected

## Palette application helper
Added `apply_palette_to_indexed_image`:
- maps indexed pixels through parsed `PaletteResource` entries
- expands Caesar II VGA-like 6-bit components to 8-bit channels
- optional transparency rule for index `0` (`index_zero_transparent`)

## Debug/report helper
Added `format_indexed_image_report` for lightweight human inspection:
- dimensions
- total pixel count
- truncated preview of palette indices

## Tests
New test suite verifies:
- successful decode of valid tiles
- truncated payload failure (out-of-bounds)
- trailing-byte failure (invalid format)
- invalid zero-dimension failure
- palette application to RGBA, including optional index-0 transparency
- report formatting/truncation marker

## Outcome
This achieves the first safe graphics decode milestone with bounded parsing and indexed output while keeping the PR tightly focused on one known simple format.
