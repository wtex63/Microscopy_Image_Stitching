# Testing Guide

## Scope

This guide validates the Win10/Win11 migration profile behavior and resilient SDK selection.

## Build Matrix

1. `Debug|x64` with default profile (`Win11`)
2. `Debug|x64` with explicit `Win11`
3. `Debug|x64` with explicit `Win10`
4. `Release|x64` with explicit `Win11`
5. `Release|x64` with explicit `Win10`

## Build Commands

Run from repository root:

```powershell
# Default profile (Win11)
msbuild .\Image_Stitching.sln /t:Build /p:Configuration=Debug;Platform=x64

# Explicit Win11 profile
msbuild .\Image_Stitching.sln /t:Build /p:Configuration=Debug;Platform=x64;TargetOsProfile=Win11

# Explicit Win10 profile
msbuild .\Image_Stitching.sln /t:Build /p:Configuration=Debug;Platform=x64;TargetOsProfile=Win10

# Release checks
msbuild .\Image_Stitching.sln /t:Build /p:Configuration=Release;Platform=x64;TargetOsProfile=Win11
msbuild .\Image_Stitching.sln /t:Build /p:Configuration=Release;Platform=x64;TargetOsProfile=Win10
```

## Runtime Smoke Test

1. Launch `x64\Debug\Image_Stitching.exe`.
2. Confirm main window is visible on startup.
3. Open two same-size images and run one stitch pass.
4. Confirm log panel shows pair progress and completion.
5. Save output and verify file is written.

## Latest Commit Validation Notes

These checks map to the latest hardening commit in this branch:

1. Correlation overflow guard:
   - Use a large image pair and confirm no abnormal displacement spikes due to numeric overflow.
2. Homography divide guard:
   - Run a difficult pair with weak overlap and confirm stitching continues without transfer/divide instability.
3. Narrow image mean sampling guard:
   - Verify no divide-by-zero when processing narrow-width images (including width < 10).
4. Feature fallback randomness:
   - Run multiple low-confidence pairs and confirm fallback behavior is not identical frame-to-frame due to reseeding.

Build verification recorded with Visual Studio 2022:

- `Debug|x64` build succeeded on VS2022 (17.14.x).

## Expected Outcomes

1. Build succeeds for both profiles when a compatible Windows 10 SDK is installed.
2. Default build profile behaves as Win11 (`TargetOsProfile=Win11`).
3. Explicit `TargetOsProfile=Win10` uses minimum OS version `10.0.19041.0`.
4. No linker lock (`LNK1104`) when app is closed before rebuild.

## Troubleshooting

1. If SDK resolution fails, install at least one Windows 10 SDK in Visual Studio Installer.
2. If an EXE lock occurs, stop `Image_Stitching.exe` and rebuild.
3. If startup fails before GUI appears, check startup log output from the application diagnostics path.
