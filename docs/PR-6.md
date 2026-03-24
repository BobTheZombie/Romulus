# PR 6: File Loading + Binary Probe Layer

## Scope
PR 6 adds a narrow bridge between validated on-disk Caesar II files and the bounded in-memory `BinaryReader` introduced in PR 5.

## What this PR adds
- Safe file loading utility:
  - `load_file_to_memory(path, max_allowed_bytes)`
  - explicit failure codes for not-found / not-regular-file / oversize / read-failed
  - default bounded load limit (`32 MiB`) to avoid accidental unbounded reads
- Reader integration helper:
  - `make_binary_reader(const LoadedFile&)` to construct a `BinaryReader` directly from loaded buffers
- Binary probe utility:
  - reports source path, size, leading signature bytes, first little-endian scalar values (when available)
  - formatting helper for probe text reports
- Optional simple header parsing (bounded):
  - detects and parses a small DOS `MZ` header summary only when magic and minimum size checks pass
  - uses `BinaryReader` seek/read bounds checks for field extraction
- CLI path:
  - `caesar2 --probe-file <path>`
  - accepts absolute paths, or resolves relative paths under `--data-dir` / resolved data root

## Tests added
- `romulus_file_loader_tests`
  - successful file round-trip load
  - oversize rejection
  - directory-path rejection
- `romulus_binary_probe_tests`
  - scalar/signature probe behavior
  - DOS `MZ` header summary parse behavior
  - probe flow from loaded file buffers

## Non-goals for this PR
- No deep Caesar II resource decoding.
- No rendering or simulation feature changes.
- No format-specific payload parsing beyond a tiny optional header summary.
