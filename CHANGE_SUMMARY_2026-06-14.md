# Change Summary - 2026-06-14

## Scope

This update adds repeatable CPU/GPU benchmarking automation, expands runtime instrumentation, improves GPU fallback performance, and introduces blend-mode controls in both CLI and UI.

## What Changed

### 1) Startup automation and CLI controls

- Added startup argument handling so the app can run unattended from command line.
- Added support for:

  - `--input <path>` (repeatable for image pairs)
  - `--cpu` / `--gpu`
  - `--autoclosepreview`
  - `--log <path>`
  - `--exit`
  - `--blend auto|quality|speed`

- Added auto-run flow on startup for benchmark scenarios.

### 2) Logging and timing instrumentation

- Extended log output with backend and stage timing details.
- Added/standardized summary lines for parsing:

  - `Phase backend:`
  - `Pair timing summary:`
  - `Fallback breakdown:`

- Added optional file logging path in batch mode.

### 3) GPU fallback optimization

- Optimized fallback edge/refine-heavy work in `Correlation.cpp`.
- Added gradient caching reuse across fallback/refine routines.
- Tuned GPU-specific search/refine budgets to reduce fallback cost while preserving matching quality.

### 4) Blend stage mode controls

- Added blend mode resolver with three modes:

  - `auto`: choose based on estimated transform quality/shape
  - `quality`: force Laplacian pyramid blend
  - `speed`: force fast translation blend

- In batch automation mode, visualization rendering is skipped during timed blend region to avoid skewed benchmark numbers.

### 5) UI support for blend mode

- Added WinForms blend-mode dropdown and label.
- Added localization resources for new UI strings in both default and Turkish resource files.

### 6) Benchmark automation scripts

- Added `scripts/speed_test_compare.bat` to run CPU and GPU tests sequentially and write logs.
- Added `scripts/append_speed_summary.ps1` to parse benchmark logs and append rows to `speed_logs/speed_summary.csv`.

## Typical Usage

```bat
scripts\speed_test_compare.bat "<img1>" "<img2>" "<optional_exe_path>" speed
```

Blend mode options:

- `auto`
- `quality`
- `speed`

## Notes

- This update significantly improves blend time in `speed` mode for translation-compatible pairs.
- `quality` mode remains the highest-fidelity blend path.
- Generated benchmark artifacts under `speed_logs/` are runtime output and can be kept local.
