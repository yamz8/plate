# Changelog

## 0.2.0 (2026-10-24)

- Paste sample names from a spreadsheet with Ctrl+V, or import them from a CSV,
  TSV, or text file with Ctrl+I. Names fill across and down from the selection,
  a header row is skipped, and names that run off the plate are counted.
- Copy the selection's labels with Ctrl+C, and clear it with Delete.
- Templates: a built-in ELISA layout, and your own saved with Ctrl+T, offered
  under New plate. They are `.plate` files in `~/.local/share/plate/templates`.
- `plate new --template`, `plate templates`, `plate names`, and
  `plate export --csv -` for standard output.
- Redo works with Ctrl+Shift+Z (it never fired) and Ctrl+Y.
- Tabbing into a dilution field selects its value, so typing replaces it.
- The shortcut overlay fits small windows, scrolling with the arrow keys.
- Danish everywhere new, and in the file chooser's filters.
- `plate skill install` takes `--json`.
- `bin/test` drives the real window and every command, not just the model.

## 0.1.0 (2026-10-23)

The first version, installed locally and never released: 96- and 384-well
layouts, six sample types with colors and patterns, serial dilutions,
replicates, undo, `.plate` files, CSV, SVG, and PDF export, and the
`new`, `check`, `export`, and `skill` commands.
