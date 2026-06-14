# Microscopy Image Stitching
This repo is a solution for combining 2D images based on Phase Only correlation and Laplacian Pyramid methods. 
This method makes accurate and fast compositing only for 2D images moving in the x and y axes.
Multiple merging can be done using datasets consisting of images. In addition, the functions parallelized using OpenMP and still under development.

## Algorithmic process  
- Phase Only Correlation 
- Feature Matching (classical fallback)
- Homography matrix
- Laplacian Pyramid 

### Matching pipeline (current)
1. Fast first pass: Phase-Only Correlation (POC) estimates displacement.
2. Confidence gate: grayscale NCC score is computed for the phase shift.
3. Fallback path (if phase confidence is low):
   - Binary feature extraction on grayscale images.
   - Descriptor matching with ratio test.
   - RANSAC translation model.
   - Confidence score from RANSAC inlier ratio.
4. Homography estimation and Laplacian pyramid blending.

### Progress and localization

- Long-running stages are logged in the UI (pair loading, shift estimation, fallback usage, blending, completion).
- Messages are localized via `Strings.resx` and `Strings.tr.resx`.

### Recent hardening notes (latest commit)

- Correlation accumulation now uses wider integer math to avoid overflow on larger overlap regions.
- Panorama transfer now guards homography division when `z` is near zero.
- Panorama background mean sampling now avoids divide-by-zero for narrow images.
- Feature fallback random seeding is now one-time per process (instead of reseeding per call).

### Windows SDK and OS profile migration notes

- The project now uses resilient SDK selection: `WindowsTargetPlatformVersion` defaults to `10.0` so MSBuild can pick an installed compatible SDK.
- The default OS profile is `Win11` (`TargetOsProfile=Win11`) with minimum version `10.0.22000.0`.
- A `Win10` compatibility profile is available via `TargetOsProfile=Win10`, with minimum version `10.0.19041.0`.
- Toolset is `v143` for all configurations.

Build examples (MSBuild):

```powershell
# Default profile (Win11)
msbuild .\Image_Stitching.sln /t:Build /p:Configuration=Debug;Platform=x64

# Explicit Win11 profile
msbuild .\Image_Stitching.sln /t:Build /p:Configuration=Debug;Platform=x64;TargetOsProfile=Win11

# Win10 profile
msbuild .\Image_Stitching.sln /t:Build /p:Configuration=Debug;Platform=x64;TargetOsProfile=Win10
```

See `TESTING_GUIDE.md` for migration verification steps.

### References and licensing notes

- Core methods (phase correlation, NCC, RANSAC, homography) are classical computer vision/statistical methods.
- The implementation in this repository is custom C++/CLI code.
- If external libraries (such as OpenCV ORB/AKAZE) are added later, include their license files and third-party notices in distribution.

### Acceleration using OpenMP
<img src="https://developers.redhat.com/blog/wp-content/uploads/2016/03/openmp_lg_transparent.gif" width="290" height="110" />

### Optional GPU phase-correlation build (OpenCV CUDA)

GPU phase correlation is now wired as an opt-in build path. Default builds remain CPU-only.

Required:

- OpenCV build that includes CUDA modules and matching runtime DLLs.
- `OPENCV_DIR` set, or pass `OpenCVDir` explicitly.

Enable in MSBuild:

```powershell
msbuild .\Image_Stitching.sln /t:Build /p:Configuration=Debug;Platform=x64;EnableOpenCVCuda=true;OpenCVDir="C:\opencv"
```

If your OpenCV world lib name/version differs, override it:

```powershell
msbuild .\Image_Stitching.sln /t:Build /p:Configuration=Debug;Platform=x64;EnableOpenCVCuda=true;OpenCVDir="C:\opencv";OpenCVLibNameDebug=opencv_world4xxxd.lib
```

Notes:

- `EnableOpenCVCuda=true` injects `USE_OPENCV_CUDA` for x64 builds.
- Include/lib/bin defaults are derived from `OpenCVDir` (`include`, `x64\vc17\lib`, `x64\vc17\bin`).
- Post-build copies `opencv*.dll` into the output folder when GPU mode is enabled.

### Optional GPU phase-correlation build (OpenCV OpenCL for AMD)

For AMD GPUs (including Radeon RX 7600), use the OpenCL path.

Required:

- OpenCV build with OpenCL support (standard OpenCV builds usually include this).
- AMD driver installed. Verified target environment example: AMD Software Adrenalin Edition 26.6.1.
- `OPENCV_DIR` set, or pass `OpenCVDir` explicitly.

Enable in MSBuild:

```powershell
msbuild .\Image_Stitching.sln /t:Build /p:Configuration=Debug;Platform=x64;EnableOpenCVOpenCL=true;OpenCVDir="C:\opencv"
```

Runtime behavior when the UI checkbox Use GPU phase backend is enabled:

- CUDA available -> uses CUDA backend.
- CUDA unavailable and OpenCL available -> uses OpenCL backend.
- Neither available -> falls back to CPU backend.

Visual Studio 2022 IDE defaults in this repository:

- `EnableOpenCVOpenCL=true` by default.
- Auto-detects OpenCV at `C:\opencv\build` when present.
- Uses `C:\opencv\build\include`, `C:\opencv\build\x64\vc16\lib`, and `C:\opencv\build\x64\vc16\bin`.
- Prefers OpenCV 4.12 world libs (`opencv_world4120d.lib`/`opencv_world4120.lib`) when found.

### It can generate panorama image even in small common areas

![image 1](https://github.com/fbasatemur/Microscopy_Image_Stitching/blob/main/sample_images/little_area.jpg)

### Scanning can be done in all directions. It can combine images that have moved asymmetrically in all directions

![image 2](https://github.com/fbasatemur/Microscopy_Image_Stitching/blob/main/sample_images/big_steps.jpg)

### Panoramic images created from sample microscope images

![image 3](https://github.com/fbasatemur/Microscopy_Image_Stitching/blob/main/sample_images/big_image1.bmp)

![image 4](https://github.com/fbasatemur/Microscopy_Image_Stitching/blob/main/sample_images/big_image2.bmp)

![image 5](https://github.com/fbasatemur/Microscopy_Image_Stitching/blob/main/sample_images/big_image3.bmp)
