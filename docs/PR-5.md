# PR 5: Bounded Binary Reader Utility

## Scope
PR 5 introduces a narrow utility layer for future Caesar II binary parsing by adding a safe, bounded, in-memory binary reader.

## What this PR adds
- New data-layer binary reader (`romulus::data::BinaryReader`) for in-memory buffers.
- Little-endian integer reads:
  - `read_u8`
  - `read_u16_le`
  - `read_u32_le`
- Offset helpers:
  - `tell`
  - `seek`
  - `remaining`
- Bounded byte span reads with `read_bytes`.
- Structured parse error type with machine-readable code and detailed message text.
- Unit tests for valid reads, bounds failure behavior, and seek/tell semantics.

## Non-goals for this PR
- No Caesar II file format decoding yet.
- No rendering, runtime, or simulation feature changes.

## Notes
- All reads are bounds-checked and return parse errors instead of silently advancing.
- Failed reads and failed seek operations preserve the reader's prior offset.
