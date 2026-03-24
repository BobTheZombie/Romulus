# PR 4: Caesar II Data File Inventory Manifest

## Scope
PR 4 adds a narrow data-inventory feature that scans an already validated Caesar II data root and emits a stable text manifest of discovered files.

## What this PR adds
- New data-layer inventory module (`romulus::data::file_inventory`) that recursively scans regular files under a validated data root.
- Per-file metadata collection:
  - filename
  - root-relative path
  - file size in bytes
  - required-known classification (based on `required_entries()` file requirements).
- Stable manifest formatting in a readable text table.
- CLI path in `caesar2`:
  - `--inventory-manifest` to generate a manifest from a validated data root
  - `--manifest-out <path>` to write the manifest to disk (otherwise prints to stdout)
- Unit tests covering discovered file reporting, required-entry visibility, and stable/sorted output.

## Non-goals for this PR
- No proprietary Caesar II file parsing.
- No rendering or simulation changes.
- No schema-heavy output format changes (JSON intentionally deferred).

## Usage
```bash
# print manifest to stdout
./caesar2 --data-dir /path/to/caesar2-data --inventory-manifest

# write manifest to a file
./caesar2 --data-dir /path/to/caesar2-data --inventory-manifest --manifest-out manifest.txt
```

## Notes
- The inventory path validates the data root before scanning; invalid roots fail with existing validation messages.
- Manifest rows are sorted by relative path to keep output stable across runs.
