# PR 7: Palette Resource Parser

## Scope
PR 7 introduces the first format-specific Caesar II resource parser by decoding palette bytes into a structured palette representation.

## What this PR adds
- Palette domain types:
  - `PaletteEntry { red, green, blue }`
  - `PaletteResource` with explicit format constants (`256` entries, `3` bytes per entry)
- Palette parser:
  - `parse_palette_resource(span<const std::byte>)`
  - `parse_palette_resource(span<const std::uint8_t>)`
  - uses `BinaryReader` for bounded read behavior
- Explicit format validation:
  - byte-size must be divisible by RGB triplets
  - entry count must be exactly 256
  - channel components must remain within VGA 6-bit range `[0, 63]`
- Structured error handling:
  - extends parse errors with `InvalidFormat` for malformed payloads
  - preserves detailed parse context (`offset`, `buffer_size`, message)
- Palette inspection helper:
  - `format_palette_report(...)` emits a concise text summary and sample entries

## Tests added
- `romulus_palette_tests`
  - successful synthetic 256-entry palette decode
  - malformed length rejection (non-triplet byte count)
  - wrong entry-count rejection
  - invalid component-range rejection
  - palette report formatting path
  - safe parse flow from bytes loaded by `load_file_to_memory`

## Non-goals for this PR
- No sprite decoding.
- No tile decoding.
- No runtime/simulation feature changes.
