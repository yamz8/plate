---
name: plate
description: |
  Design and export microplate layouts (96- or 384-well) with Plate. Use for ANY
  request to lay out, change, check, or export a plate map, plate layout, assay
  plate, serial dilution plan, or a .plate file, or to make a CSV for a plate
  reader or liquid handler.
---

# Plate

A plate layout is one `.plate` JSON file. You write it with your ordinary
editing tools; the `plate` command checks and exports it. No command needs a
display.

## Format

```json
{
  "format": "plate",
  "version": 1,
  "rows": 8,
  "columns": 12,
  "unit": "µM",
  "wells": [
    { "well": "A1", "type": "standard", "label": "Std 1", "concentration": 100 },
    { "well": "A2", "type": "standard", "label": "Std 2", "concentration": 50 },
    { "well": "B1", "type": "sample", "label": "Lysate 3" },
    { "well": "H12", "type": "blank" }
  ]
}
```

- `rows` × `columns` is 8 × 12 (96 wells) or 16 × 24 (384 wells). Rows are
  letters (A–H or A–P) and columns numbers from 1, so wells are `A1`…`P24`.
- `type` is one of `sample`, `standard`, `control+`, `control-`, `blank`,
  `empty`. Wells left out are empty.
- `label` is optional text; `concentration` is an optional number, zero or
  more, in the plate's `unit`.
- List each well once. Writing them in plate order (A1, A2, … B1) keeps diffs
  readable.

## Workflow

1. Start: `plate new assay.plate --wells 96 --unit µM`, or edit an existing file.
2. Edit the JSON directly. For a serial dilution, compute each concentration
   (start / factor^step) and write it per well.
3. Check: `plate check assay.plate --json` lists every problem with its well.
   Fix all of them.
4. Export when asked:
   - `plate export assay.plate --csv assay.csv`: Well, Type, Label,
     Concentration for every well, for plate readers and liquid handlers.
   - `--svg map.svg` or `--pdf map.pdf`: a printable plate map with a legend.
   Several at once are fine.

## Working with the person

- `plate open <file>` opens the window for them; it needs their desktop, so do
  not wait on it.
- Every command takes `--json`, exits 0 on success and 1 on failure, and writes
  errors to stderr. `plate help <command>` lists a command's options.
