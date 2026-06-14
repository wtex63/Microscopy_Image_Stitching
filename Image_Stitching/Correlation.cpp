#include "Correlation.h"
#include "Process.h"
#include <math.h>
#include <vector>
#include <chrono>
#include <atomic>
#include <cstdint>

#if defined(USE_OPENCV_CUDA) || defined(USE_OPENCV_OPENCL)
#include <opencv2/core.hpp>
#include <opencv2/core/ocl.hpp>
#include <opencv2/imgproc.hpp>
#endif

#if defined(USE_OPENCV_CUDA)
#include <opencv2/core/cuda.hpp>
#include <opencv2/cudaarithm.hpp>
#endif

static constexpr int kRotatedFallbackBudgetMs = 2200;
static constexpr int kRotatedFallbackBudgetGpuMs = 1500;


double* Conjugate(double* imaginary, int width, int height) {

	long imageSize = width * height;
	double* conjugated = new double[imageSize];

#pragma omp parallel shared(imageSize, conjugated, imaginary)
	{
#pragma omp for schedule(static) nowait
		for (long i = 0; i < imageSize; i++)
			conjugated[i] = -imaginary[i];
	}

	return conjugated;
}

// Global toggle for hard overlap constraint. Default enabled.
static std::atomic<bool> g_hardOverlapCheckEnabled(true);
static std::atomic<int> g_phaseCorrelationBackend((int)PhaseCorrelationBackendCpu);

#if defined(USE_OPENCV_CUDA)
static constexpr bool kPhaseCorrelationGpuCompiled = true;
#else
static constexpr bool kPhaseCorrelationGpuCompiled = false;
#endif

#if defined(USE_OPENCV_CUDA) || defined(USE_OPENCV_OPENCL)
static constexpr bool kPhaseCorrelationOpenClCompiled = true;
#else
static constexpr bool kPhaseCorrelationOpenClCompiled = false;
#endif

static bool IsPhaseCorrelationOpenClAvailableImpl() {
#if defined(USE_OPENCV_CUDA) || defined(USE_OPENCV_OPENCL)
	try {
		if (!cv::ocl::haveOpenCL())
			return false;
		cv::ocl::setUseOpenCL(true);
		return cv::ocl::useOpenCL();
	}
	catch (...) {
		return false;
	}
#else
	return false;
#endif
}

void SetHardOverlapCheck(bool enabled) {
	g_hardOverlapCheckEnabled.store(enabled);
}

bool GetHardOverlapCheck() {
	return g_hardOverlapCheckEnabled.load();
}

void SetPhaseCorrelationBackend(PhaseCorrelationBackend backend) {
	g_phaseCorrelationBackend.store((int)backend);
}

PhaseCorrelationBackend GetPhaseCorrelationBackend() {
	int val = g_phaseCorrelationBackend.load();
	if (val == (int)PhaseCorrelationBackendGpu)
		return PhaseCorrelationBackendGpu;
	return PhaseCorrelationBackendCpu;
}

bool IsPhaseCorrelationGpuAvailable() {
	#if defined(USE_OPENCV_CUDA)
	try {
		if (cv::cuda::getCudaEnabledDeviceCount() > 0)
			return true;
	}
	catch (...) {
		// Continue to OpenCL probe below.
	}
	#endif

	if (IsPhaseCorrelationOpenClAvailableImpl())
		return true;

	return kPhaseCorrelationGpuCompiled || kPhaseCorrelationOpenClCompiled;
}

const char* GetPhaseCorrelationBackendName() {
	if (GetPhaseCorrelationBackend() == PhaseCorrelationBackendGpu) {
		#if defined(USE_OPENCV_CUDA)
		try {
			if (cv::cuda::getCudaEnabledDeviceCount() > 0)
				return "GPU (CUDA)";
		}
		catch (...) {
			// Fall through to OpenCL probe.
		}
		#endif

		if (IsPhaseCorrelationOpenClAvailableImpl())
			return "GPU (OpenCL)";

		return "GPU requested (fallback CPU)";
	}
	return "CPU";
}

void ComplexMult(double* fft1Real, double* fft1Imag, double* fft2Real, double* fft2Imag, double* outReal, double* outImag, int width, int height) {

	long imageSize = width * height;

#pragma omp parallel shared(imageSize, fft1Real, fft2Real, fft1Imag, fft2Imag, outReal, outImag)
	{
#pragma omp for schedule(static) nowait
		for (long i = 0; i < imageSize; i++)
		{
			outReal[i] = fft1Real[i] * fft2Real[i] - fft1Imag[i] * fft2Imag[i];
			outImag[i] = fft1Real[i] * fft2Imag[i] + fft1Imag[i] * fft2Real[i];
		}
	}
}

#if defined(USE_OPENCV_CUDA)
static bool TryPhaseCorrelationGpuCuda(double* fft1Real, double* fft1Imag, double* fft2Real, double* fft2Imag, int width, int height, double*& outReal)
{
	if (fft1Real == nullptr || fft1Imag == nullptr || fft2Real == nullptr || fft2Imag == nullptr || width <= 0 || height <= 0)
		return false;

	try {
		cv::Mat spec1(height, width, CV_32FC2);
		cv::Mat spec2(height, width, CV_32FC2);

		for (int y = 0; y < height; y++) {
			cv::Vec2f* row1 = spec1.ptr<cv::Vec2f>(y);
			cv::Vec2f* row2 = spec2.ptr<cv::Vec2f>(y);
			for (int x = 0; x < width; x++) {
				const long idx = (long)y * (long)width + (long)x;
				row1[x][0] = (float)fft1Real[idx];
				row1[x][1] = (float)fft1Imag[idx];
				row2[x][0] = (float)fft2Real[idx];
				row2[x][1] = (float)fft2Imag[idx];
			}
		}

		cv::cuda::GpuMat dSpec1, dSpec2, dCross, dIfft;
		dSpec1.upload(spec1);
		dSpec2.upload(spec2);

		// Cross-power spectrum: F1 * conj(F2)
		cv::cuda::mulSpectrums(dSpec1, dSpec2, dCross, 0, true);

		std::vector<cv::cuda::GpuMat> channels;
		cv::cuda::split(dCross, channels);
		if (channels.size() != 2)
			return false;

		cv::cuda::GpuMat dMag;
		cv::cuda::magnitude(channels[0], channels[1], dMag);
		cv::cuda::max(dMag, cv::Scalar(1e-12f), dMag);
		cv::cuda::divide(channels[0], dMag, channels[0]);
		cv::cuda::divide(channels[1], dMag, channels[1]);
		cv::cuda::merge(channels, dCross);

		cv::cuda::dft(dCross, dIfft, cv::Size(width, height), cv::DFT_INVERSE | cv::DFT_SCALE);

		cv::Mat ifftHost;
		dIfft.download(ifftHost);

		const long imageSize = (long)width * (long)height;
		outReal = new double[imageSize];
		for (int y = 0; y < height; y++) {
			const cv::Vec2f* row = ifftHost.ptr<cv::Vec2f>(y);
			for (int x = 0; x < width; x++) {
				const long idx = (long)y * (long)width + (long)x;
				outReal[idx] = (double)row[x][0];
			}
		}

		return true;
	}
	catch (...) {
		outReal = nullptr;
		return false;
	}
}
#endif

#if defined(USE_OPENCV_CUDA) || defined(USE_OPENCV_OPENCL)
static bool TryPhaseCorrelationOpenClUmat(double* fft1Real, double* fft1Imag, double* fft2Real, double* fft2Imag, int width, int height, double*& outReal)
{
	if (fft1Real == nullptr || fft1Imag == nullptr || fft2Real == nullptr || fft2Imag == nullptr || width <= 0 || height <= 0)
		return false;
	if (!IsPhaseCorrelationOpenClAvailableImpl())
		return false;

	try {
		cv::Mat spec1(height, width, CV_32FC2);
		cv::Mat spec2(height, width, CV_32FC2);

		for (int y = 0; y < height; y++) {
			cv::Vec2f* row1 = spec1.ptr<cv::Vec2f>(y);
			cv::Vec2f* row2 = spec2.ptr<cv::Vec2f>(y);
			for (int x = 0; x < width; x++) {
				const long idx = (long)y * (long)width + (long)x;
				row1[x][0] = (float)fft1Real[idx];
				row1[x][1] = (float)fft1Imag[idx];
				row2[x][0] = (float)fft2Real[idx];
				row2[x][1] = (float)fft2Imag[idx];
			}
		}

		cv::UMat uSpec1, uSpec2, uCross, uIfft;
		spec1.copyTo(uSpec1);
		spec2.copyTo(uSpec2);

		cv::mulSpectrums(uSpec1, uSpec2, uCross, 0, true);

		std::vector<cv::UMat> channels;
		cv::split(uCross, channels);
		if (channels.size() != 2)
			return false;

		cv::UMat uMag;
		cv::magnitude(channels[0], channels[1], uMag);
		cv::max(uMag, cv::Scalar(1e-12f), uMag);
		cv::divide(channels[0], uMag, channels[0]);
		cv::divide(channels[1], uMag, channels[1]);
		cv::merge(channels, uCross);

		cv::dft(uCross, uIfft, cv::DFT_INVERSE | cv::DFT_SCALE);

		cv::Mat ifftHost;
		uIfft.copyTo(ifftHost);

		const long imageSize = (long)width * (long)height;
		outReal = new double[imageSize];
		for (int y = 0; y < height; y++) {
			const cv::Vec2f* row = ifftHost.ptr<cv::Vec2f>(y);
			for (int x = 0; x < width; x++) {
				const long idx = (long)y * (long)width + (long)x;
				outReal[idx] = (double)row[x][0];
			}
		}

		return true;
	}
	catch (...) {
		outReal = nullptr;
		return false;
	}
}
#endif

double* PhaseCorrelation(double* fft1Real, double* fft1Imag, double* fft2Real, double* fft2Imag, int width, int height)
{
	// GPU backend can be enabled at runtime once compiled-in support is available.
	// Current default build safely falls back to CPU.
	if (GetPhaseCorrelationBackend() == PhaseCorrelationBackendGpu && !IsPhaseCorrelationGpuAvailable()) {
		// Keep CPU fallback silent and deterministic for now.
	}

	#if defined(USE_OPENCV_CUDA)
	if (GetPhaseCorrelationBackend() == PhaseCorrelationBackendGpu && IsPhaseCorrelationGpuAvailable()) {
		double* gpuOut = nullptr;
		if (TryPhaseCorrelationGpuCuda(fft1Real, fft1Imag, fft2Real, fft2Imag, width, height, gpuOut) && gpuOut != nullptr)
			return gpuOut;
	}
	#endif

	#if defined(USE_OPENCV_CUDA) || defined(USE_OPENCV_OPENCL)
	if (GetPhaseCorrelationBackend() == PhaseCorrelationBackendGpu) {
		double* openClOut = nullptr;
		if (TryPhaseCorrelationOpenClUmat(fft1Real, fft1Imag, fft2Real, fft2Imag, width, height, openClOut) && openClOut != nullptr)
			return openClOut;
	}
	#endif

	double* multReal, * multImag;
	long imageSize = width * height;

	multReal = new double[imageSize];
	multImag = new double[imageSize];

	double* conjugated = Conjugate(fft2Imag, width, height);
	ComplexMult(fft1Real, fft1Imag, fft2Real, conjugated, multReal, multImag, width, height);

	double norm;
#pragma omp parallel shared(multReal, multImag, imageSize) private(norm)
	{
#pragma omp for schedule(static) nowait
		for (long i = 0; i < imageSize; i++)
		{
			norm = sqrt(pow(multReal[i], 2) + pow(multImag[i], 2));
			if (norm > 1e-12) {
				multReal[i] /= norm;
				multImag[i] /= norm;
			}
			else {
				multReal[i] = 0.0;
				multImag[i] = 0.0;
			}
		}
	}

	double* outReal = new double[imageSize];
	double* outImag = new double[imageSize];

	IFFT2D(outReal, outImag, multReal, multImag, width, height);


	delete[] multReal; delete[] multImag;
	delete[] outImag; delete[] conjugated;

	return outReal;
}

float Correlation(BYTE* img1, BYTE* img2, int width, int height, int zoneWidth, int zoneHeight, int start1H, int start1W, int start2H, int start2W) {

	int newR1, newC1, newR2, newC2;
	uint64_t total1 = 0, total2 = 0;
	double total = 0.0, totalSqr1 = 0.0, totalSqr2 = 0.0;
	double mean1, mean2;

#pragma omp parallel shared(zoneHeight,zoneWidth,start1H,start2H,start1W,start2W, width, img1, img2) private(newR1, newR2, newC1, newC2)
	{
#pragma omp for schedule(static) reduction(+:total1,total2) nowait
		for (int r = 0; r < zoneHeight; r++) {

			newR1 = r + start1H;
			newR2 = r + start2H;
			for (int c = 0; c < zoneWidth; c++) {

				newC1 = c + start1W;
				newC2 = c + start2W;
				total1 += uint64_t(img1[newR1 * width + newC1]);
				total2 += uint64_t(img2[newR2 * width + newC2]);
			}
		}
	}

	mean1 = double(total1) / double(zoneHeight * zoneWidth);
	mean2 = double(total2) / double(zoneHeight * zoneWidth);

#pragma omp parallel shared(zoneHeight,zoneWidth,start1H,start2H,start1W,start2W,width,img1,img2,mean1,mean2) private(newR1, newR2, newC1, newC2)
	{
#pragma omp for schedule(static) reduction(+:total,totalSqr1,totalSqr2) nowait
		for (int r = 0; r < zoneHeight; r++) {

			newR1 = r + start1H;
			newR2 = r + start2H;
			for (int c = 0; c < zoneWidth; c++) {

				newC1 = c + start1W;
				newC2 = c + start2W;
				total += ((img1[newR1 * width + newC1] - mean1) * (img2[newR2 * width + newC2] - mean2));

				totalSqr1 += pow((img1[newR1 * width + newC1] - mean1), 2);
				totalSqr2 += pow((img2[newR2 * width + newC2] - mean2), 2);
			}
		}
	}

	const double denom = sqrt(totalSqr1 * totalSqr2);
	if (denom <= 1e-12) {
		return 0.0f;
	}

	return (float)(total / denom);
}

static float EvaluateShiftNccSampled(BYTE* img1, BYTE* img2, int width, int height, int dx, int dy, int sampleStep)
{
	if (img1 == nullptr || img2 == nullptr || width <= 1 || height <= 1)
		return -2.0f;

	if (sampleStep < 1)
		sampleStep = 1;

	const int xStart = (dx > 0) ? dx : 0;
	const int yStart = (dy > 0) ? dy : 0;
	const int xEnd = (width + dx - 1 < width - 1) ? (width + dx - 1) : (width - 1);
	const int yEnd = (height + dy - 1 < height - 1) ? (height + dy - 1) : (height - 1);

	if (xStart >= xEnd || yStart >= yEnd)
		return -2.0f;

	const int overlapW = xEnd - xStart + 1;
	const int overlapH = yEnd - yStart + 1;
	// Accept edge-overlap cases (small overlap bands) so stitching can align adjacent captures.
	if (overlapW < width / 12 || overlapH < height / 12)
		return -2.0f;

	double s1 = 0.0, s2 = 0.0, s11 = 0.0, s22 = 0.0, s12 = 0.0;
	int n = 0;

	for (int y = yStart; y <= yEnd; y += sampleStep) {
		const int y2 = y - dy;
		const int row1 = y * width;
		const int row2 = y2 * width;
		for (int x = xStart; x <= xEnd; x += sampleStep) {
			const int x2 = x - dx;
			double a = (double)img1[row1 + x];
			double b = (double)img2[row2 + x2];
			s1 += a;
			s2 += b;
			s11 += a * a;
			s22 += b * b;
			s12 += a * b;
			n++;
		}
	}

	if (n < 64)
		return -2.0f;

	const double nf = (double)n;
	const double num = s12 - (s1 * s2) / nf;
	const double den1 = s11 - (s1 * s1) / nf;
	const double den2 = s22 - (s2 * s2) / nf;
	const double den = sqrt(den1 * den2);

	if (den <= 1e-12)
		return -2.0f;

	return (float)(num / den);
}

static float ShiftSelectionBias(int width, int height, int dx, int dy)
{
	const float ndx = (float)abs(dx) / (float)(width > 0 ? width : 1);
	const float ndy = (float)abs(dy) / (float)(height > 0 ? height : 1);
	const float dominant = (ndx > ndy) ? ndx : ndy;
	const float secondary = (ndx > ndy) ? ndy : ndx;

	// Favor meaningful movement and axis-dominant shifts for row/column capture grids.
	float bias = 0.10f * dominant + 0.04f * (dominant - secondary);

	// Heavily penalize tiny shifts that usually create ghost overlays.
	if (dominant < 0.05f)
		bias -= 0.30f;
	else if (dominant < 0.08f)
		bias -= 0.12f;

	return bias;
}

static float EdgeOverlapTargetBias(int width, int height, int dx, int dy)
{
	int absDx = abs(dx);
	int absDy = abs(dy);
	int overlapW = width - absDx;
	int overlapH = height - absDy;

	if (overlapW <= 0 || overlapH <= 0)
		return -1.0f;

	bool horizontalNeighbor = absDx >= absDy;
	int primaryOverlap = horizontalNeighbor ? overlapW : overlapH;
	int primaryDim = horizontalNeighbor ? width : height;

	// Use an image-size-aware overlap target; fixed pixel targets over-favor large overlaps on PCB scans.
	float targetOverlapPx = 0.16f * (float)primaryDim;
	if (targetOverlapPx < 80.0f) targetOverlapPx = 80.0f;
	if (targetOverlapPx > 260.0f) targetOverlapPx = 260.0f;
	float diff = (float)fabs((float)primaryOverlap - targetOverlapPx);
	float bias = -0.45f * (diff / targetOverlapPx);

	if (primaryOverlap > (int)(targetOverlapPx * 2.0f))
		bias -= 0.30f;
	if (primaryOverlap < (int)(targetOverlapPx * 0.55f))
		bias -= 0.20f;

	int dominantShift = horizontalNeighbor ? absDx : absDy;
	int dominantDim = primaryDim;
	if (dominantShift < dominantDim / 10)
		bias -= 0.25f;
	if (dominantShift < dominantDim / 6)
		bias -= 0.30f;
	if (dominantShift < dominantDim / 4)
		bias -= 0.18f;

	return bias;
}

static float EvaluateShiftNccSampledRotated(BYTE* img1, BYTE* img2, int width, int height, int dx, int dy, float angleDeg, int sampleStep)
{
	if (img1 == nullptr || img2 == nullptr || width <= 1 || height <= 1)
		return -2.0f;

	if (sampleStep < 1)
		sampleStep = 1;

	const double rad = angleDeg * (3.14159265358979323846 / 180.0);
	const double ca = cos(rad);
	const double sa = sin(rad);
	const double cx = 0.5 * (double)(width - 1);
	const double cy = 0.5 * (double)(height - 1);

	double s1 = 0.0, s2 = 0.0, s11 = 0.0, s22 = 0.0, s12 = 0.0;
	int n = 0;

	for (int y = 0; y < height; y += sampleStep) {
		const int row1 = y * width;
		for (int x = 0; x < width; x += sampleStep) {
			double px = (double)x - (double)dx;
			double py = (double)y - (double)dy;

			double tx = px - cx;
			double ty = py - cy;

			double rx = tx * ca + ty * sa;
			double ry = -tx * sa + ty * ca;

			double x2f = rx + cx;
			double y2f = ry + cy;

			if (x2f < 0.0 || y2f < 0.0 || x2f >= (double)(width - 1) || y2f >= (double)(height - 1))
				continue;

			int x0 = (int)x2f;
			int y0 = (int)y2f;
			double fx = x2f - (double)x0;
			double fy = y2f - (double)y0;

			double v00 = (double)img2[y0 * width + x0];
			double v10 = (double)img2[y0 * width + (x0 + 1)];
			double v01 = (double)img2[(y0 + 1) * width + x0];
			double v11 = (double)img2[(y0 + 1) * width + (x0 + 1)];

			double b = (1.0 - fx) * (1.0 - fy) * v00 + fx * (1.0 - fy) * v10 + (1.0 - fx) * fy * v01 + fx * fy * v11;
			double a = (double)img1[row1 + x];

			s1 += a;
			s2 += b;
			s11 += a * a;
			s22 += b * b;
			s12 += a * b;
			n++;
		}
	}

	if (n < 64)
		return -2.0f;

	const double nf = (double)n;
	const double num = s12 - (s1 * s2) / nf;
	const double den1 = s11 - (s1 * s1) / nf;
	const double den2 = s22 - (s2 * s2) / nf;
	const double den = sqrt(den1 * den2);
	if (den <= 1e-12)
		return -2.0f;

	return (float)(num / den);
}

static void BuildGradientImage(BYTE* src, int width, int height, std::vector<BYTE>& grad)
{
	grad.assign((size_t)width * (size_t)height, (BYTE)0);
	if (src == nullptr || width < 3 || height < 3)
		return;

	for (int y = 1; y < height - 1; y++) {
		int row = y * width;
		for (int x = 1; x < width - 1; x++) {
			int idx = row + x;
			int gx = abs((int)src[idx + 1] - (int)src[idx - 1]);
			int gy = abs((int)src[idx + width] - (int)src[idx - width]);
			int g = gx + gy;
			if (g > 255) g = 255;
			grad[(size_t)idx] = (BYTE)g;
		}
	}
}

static int GetRotatedFallbackBudgetMs()
{
	if (GetPhaseCorrelationBackend() == PhaseCorrelationBackendGpu)
		return kRotatedFallbackBudgetGpuMs;
	return kRotatedFallbackBudgetMs;
}

static std::vector<BYTE>& GetGradientImageCached(BYTE* src, int width, int height)
{
	struct GradientCacheEntry {
		BYTE* src = nullptr;
		int width = 0;
		int height = 0;
		std::vector<BYTE> grad;
	};

	static GradientCacheEntry cache[2];
	static int nextEvict = 0;

	for (int i = 0; i < 2; i++) {
		if (cache[i].src == src && cache[i].width == width && cache[i].height == height)
			return cache[i].grad;
	}

	int slot = nextEvict;
	nextEvict = (nextEvict + 1) & 1;
	cache[slot].src = src;
	cache[slot].width = width;
	cache[slot].height = height;
	BuildGradientImage(src, width, height, cache[slot].grad);
	return cache[slot].grad;
}

static void FallbackGlobalNccSearch(BYTE* img1, BYTE* img2, int width, int height, int& dx, int& dy, float& angleDeg, float baseScore)
{
	if (img1 == nullptr || img2 == nullptr || width <= 1 || height <= 1)
		return;

	int ds = (width > height ? width : height) / 320;
	if (ds < 1)
		ds = 1;

	int w2 = width / ds;
	int h2 = height / ds;
	if (w2 < 40 || h2 < 40) {
		w2 = width;
		h2 = height;
		ds = 1;
	}

	std::vector<BYTE> a2((size_t)w2 * (size_t)h2);
	std::vector<BYTE> b2((size_t)w2 * (size_t)h2);
	for (int y = 0; y < h2; y++) {
		const int oy = y * ds;
		for (int x = 0; x < w2; x++) {
			const int ox = x * ds;
			a2[(size_t)y * (size_t)w2 + (size_t)x] = img1[oy * width + ox];

	bool useOpenClCoarse = false;
#if defined(USE_OPENCV_OPENCL) || defined(USE_OPENCV_CUDA)
	cv::UMat uA2;
	cv::UMat uB2;
	if (GetPhaseCorrelationBackend() == PhaseCorrelationBackendGpu && IsPhaseCorrelationOpenClAvailableImpl()) {
		try {
			cv::Mat mA2(h2, w2, CV_8UC1, a2.data());
			cv::Mat mB2(h2, w2, CV_8UC1, b2.data());
			mA2.copyTo(uA2);
			mB2.copyTo(uB2);
			useOpenClCoarse = true;
		}
		catch (...) {
			useOpenClCoarse = false;
		}
	}
#endif

	auto EvaluateCoarseScore = [&](int sdx, int sdy) -> float {
		if (sdx <= -w2 + 1 || sdx >= w2 || sdy <= -h2 + 1 || sdy >= h2)
			return -2.0f;
		if (!useOpenClCoarse)
			return EvaluateShiftNccSampled(a2.data(), b2.data(), w2, h2, sdx, sdy, 2);
#if defined(USE_OPENCV_OPENCL) || defined(USE_OPENCV_CUDA)
		try {
			const int xStart = (sdx > 0) ? sdx : 0;
			const int yStart = (sdy > 0) ? sdy : 0;
			const int xEnd = (w2 + sdx - 1 < w2 - 1) ? (w2 + sdx - 1) : (w2 - 1);
			const int yEnd = (h2 + sdy - 1 < h2 - 1) ? (h2 + sdy - 1) : (h2 - 1);
			if (xStart >= xEnd || yStart >= yEnd)
				return -2.0f;
			const int overlapW = xEnd - xStart + 1;
			const int overlapH = yEnd - yStart + 1;
			if (overlapW < w2 / 12 || overlapH < h2 / 12)
				return -2.0f;
			cv::Rect roi1(xStart, yStart, overlapW, overlapH);
			cv::Rect roi2(xStart - sdx, yStart - sdy, overlapW, overlapH);
			cv::UMat t1 = uA2(roi1);
			cv::UMat t2 = uB2(roi2);
			cv::UMat result;
			cv::matchTemplate(t1, t2, result, cv::TM_CCOEFF_NORMED);
			cv::Mat host;
			result.copyTo(host);
			if (host.empty())
				return -2.0f;
			return host.at<float>(0, 0);
		}
		catch (...) {
			return EvaluateShiftNccSampled(a2.data(), b2.data(), w2, h2, sdx, sdy, 2);
		}
#else
		return EvaluateShiftNccSampled(a2.data(), b2.data(), w2, h2, sdx, sdy, 2);
#endif
	};
			b2[(size_t)y * (size_t)w2 + (size_t)x] = img2[oy * width + ox];
		}
	}

	auto IsBetterCandidate = [](float objA, float scoreA, int dxA, int dyA, float angA,
		float objB, float scoreB, int dxB, int dyB, float angB) -> bool {
		if (objA > objB) return true;
		if (objA < objB) return false;
		if (scoreA > scoreB) return true;
		if (scoreA < scoreB) return false;
		float absAngA = (float)fabs(angA);
		float absAngB = (float)fabs(angB);
		if (absAngA < absAngB) return true;
		if (absAngA > absAngB) return false;
		int magA = abs(dxA) + abs(dyA);
		int magB = abs(dxB) + abs(dyB);
		if (magA < magB) return true;
		if (magA > magB) return false;
		if (dyA < dyB) return true;
		if (dyA > dyB) return false;
		return dxA < dxB;
	};

	int bestDx2 = 0;
	int bestDy2 = 0;
	float best2 = -2.0f;
	float bestObjective2 = -100.0f;
	float bestAngle2 = 0.0f;
	const int coarseStep2 = 4;
	const float angleCandidates[] = { -4.0f, -2.0f, 0.0f, 2.0f, 4.0f };
	auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kRotatedFallbackBudgetMs);
	std::atomic<bool> stopSearch(false);

#pragma omp parallel
	{
		int tBestDx2 = bestDx2;
		int tBestDy2 = bestDy2;
		float tBest2 = best2;
		float tBestObjective2 = bestObjective2;
		float tBestAngle2 = bestAngle2;
		int tIter = 0;

		#pragma omp for schedule(static) nowait
		for (int cy2 = -h2 / 2; cy2 <= h2 / 2; cy2 += coarseStep2) {
			if (stopSearch.load())
				continue;
			for (int ai = 0; ai < 5; ai++) {
				if (stopSearch.load())
					break;
				for (int cx2 = -w2 / 2; cx2 <= w2 / 2; cx2 += coarseStep2) {
					if (stopSearch.load())
						break;
					if ((++tIter & 63) == 0 && std::chrono::steady_clock::now() >= deadline) {
						stopSearch.store(true);
						break;
					}
					float candAngle = angleCandidates[ai];
					float s = (candAngle == 0.0f)
						? EvaluateShiftNccSampled(a2.data(), b2.data(), w2, h2, cx2, cy2, 2)
						: EvaluateShiftNccSampledRotated(a2.data(), b2.data(), w2, h2, cx2, cy2, candAngle, 2);
					int cdx = cx2 * ds;
					int cdy = cy2 * ds;
					float objective = s + ShiftSelectionBias(width, height, cdx, cdy) - (float)fabs(candAngle) * 0.005f;
					if (IsBetterCandidate(objective, s, cdx, cdy, candAngle, tBestObjective2, tBest2, tBestDx2 * ds, tBestDy2 * ds, tBestAngle2)) {
						tBest2 = s;
						tBestObjective2 = objective;
						tBestDx2 = cx2;
						tBestDy2 = cy2;
						tBestAngle2 = candAngle;
					}
				}
			}
		}

#pragma omp critical
		{
			if (IsBetterCandidate(tBestObjective2, tBest2, tBestDx2 * ds, tBestDy2 * ds, tBestAngle2,
				bestObjective2, best2, bestDx2 * ds, bestDy2 * ds, bestAngle2)) {
				best2 = tBest2;
				bestObjective2 = tBestObjective2;
				bestDx2 = tBestDx2;
				bestDy2 = tBestDy2;
				bestAngle2 = tBestAngle2;
			}
		}
	}

	int candidateDx = bestDx2 * ds;
	int candidateDy = bestDy2 * ds;
	int bestDx = candidateDx;
	int bestDy = candidateDy;
	float best = baseScore;
	float bestObjective = baseScore + ShiftSelectionBias(width, height, dx, dy);
	float bestAngle = bestAngle2;

#pragma omp parallel
	{
		int tBestDx = bestDx;
		int tBestDy = bestDy;
		float tBest = best;
		float tBestObjective = bestObjective;
		int tIter = 0;

		#pragma omp for schedule(static) nowait
		for (int cy = candidateDy - 16; cy <= candidateDy + 16; cy++) {
			if (stopSearch.load())
				continue;
			for (int cx = candidateDx - 16; cx <= candidateDx + 16; cx++) {
				if (stopSearch.load())
					break;
				if ((++tIter & 63) == 0 && std::chrono::steady_clock::now() >= deadline) {
					stopSearch.store(true);
					break;
				}
				float s = (bestAngle2 == 0.0f)
					? EvaluateShiftNccSampled(img1, img2, width, height, cx, cy, 8)
					: EvaluateShiftNccSampledRotated(img1, img2, width, height, cx, cy, bestAngle2, 8);
				float objective = s + ShiftSelectionBias(width, height, cx, cy) - (float)fabs(bestAngle2) * 0.005f;
				if (IsBetterCandidate(objective, s, cx, cy, bestAngle2, tBestObjective, tBest, tBestDx, tBestDy, bestAngle2)) {
					tBest = s;
					tBestObjective = objective;
					tBestDx = cx;
					tBestDy = cy;
				}
			}
		}

#pragma omp critical
		{
			if (IsBetterCandidate(tBestObjective, tBest, tBestDx, tBestDy, bestAngle2, bestObjective, best, bestDx, bestDy, bestAngle2)) {
				best = tBest;
				bestObjective = tBestObjective;
				bestDx = tBestDx;
				bestDy = tBestDy;
			}
		}
	}

	if (best > baseScore + 0.03f) {
		dx = bestDx;
		dy = bestDy;
		angleDeg = bestAngle;
	}
}

static void FallbackLocalNccSearch(BYTE* img1, BYTE* img2, int width, int height, int& dx, int& dy, float baseScore)
{
	const int radiusX = (width / 5 > 20) ? width / 5 : 20;
	const int radiusY = (height / 5 > 20) ? height / 5 : 20;
	const int coarseStep = 6;

	int minDx = dx - radiusX;
	int maxDx = dx + radiusX;
	int minDy = dy - radiusY;
	int maxDy = dy + radiusY;

	if (minDx < -width + 1) minDx = -width + 1;
	if (maxDx > width - 1) maxDx = width - 1;
	if (minDy < -height + 1) minDy = -height + 1;
	if (maxDy > height - 1) maxDy = height - 1;

	auto IsBetterCandidate = [](float objA, float scoreA, int dxA, int dyA,
		float objB, float scoreB, int dxB, int dyB) -> bool {
		if (objA > objB) return true;
		if (objA < objB) return false;
		if (scoreA > scoreB) return true;
		if (scoreA < scoreB) return false;
		int magA = abs(dxA) + abs(dyA);
		int magB = abs(dxB) + abs(dyB);
		if (magA < magB) return true;
		if (magA > magB) return false;
		if (dyA < dyB) return true;
		if (dyA > dyB) return false;
		return dxA < dxB;
	};

	float best = baseScore;
	float bestObjective = baseScore + ShiftSelectionBias(width, height, dx, dy);
	int bestDx = dx;
	int bestDy = dy;

	bool useOpenClCoarse = false;
#if defined(USE_OPENCV_OPENCL) || defined(USE_OPENCV_CUDA)
	cv::UMat uImg1;
	cv::UMat uImg2;
	if (GetPhaseCorrelationBackend() == PhaseCorrelationBackendGpu && IsPhaseCorrelationOpenClAvailableImpl()) {
		try {
			cv::Mat m1(height, width, CV_8UC1, img1);
			cv::Mat m2(height, width, CV_8UC1, img2);
			m1.copyTo(uImg1);
			m2.copyTo(uImg2);
			useOpenClCoarse = true;
		}
		catch (...) {
			useOpenClCoarse = false;
		}
	}
#endif

	auto EvaluateLocalCoarse = [&](int sdx, int sdy) -> float {
		if (!useOpenClCoarse)
			return EvaluateShiftNccSampled(img1, img2, width, height, sdx, sdy, 4);
#if defined(USE_OPENCV_OPENCL) || defined(USE_OPENCV_CUDA)
		try {
			const int xStart = (sdx > 0) ? sdx : 0;
			const int yStart = (sdy > 0) ? sdy : 0;
			const int xEnd = (width + sdx - 1 < width - 1) ? (width + sdx - 1) : (width - 1);
			const int yEnd = (height + sdy - 1 < height - 1) ? (height + sdy - 1) : (height - 1);
			if (xStart >= xEnd || yStart >= yEnd)
				return -2.0f;
			const int overlapW = xEnd - xStart + 1;
			const int overlapH = yEnd - yStart + 1;
			if (overlapW < width / 12 || overlapH < height / 12)
				return -2.0f;
			cv::Rect roi1(xStart, yStart, overlapW, overlapH);
			cv::Rect roi2(xStart - sdx, yStart - sdy, overlapW, overlapH);
			cv::UMat t1 = uImg1(roi1);
			cv::UMat t2 = uImg2(roi2);
			cv::UMat s1, s2, result;
			cv::resize(t1, s1, cv::Size(), 0.25, 0.25, cv::INTER_AREA);
			cv::resize(t2, s2, cv::Size(), 0.25, 0.25, cv::INTER_AREA);
			if (s1.cols < 8 || s1.rows < 8 || s2.cols < 8 || s2.rows < 8)
				return EvaluateShiftNccSampled(img1, img2, width, height, sdx, sdy, 4);
			cv::matchTemplate(s1, s2, result, cv::TM_CCOEFF_NORMED);
			cv::Mat host;
			result.copyTo(host);
			if (host.empty())
				return -2.0f;
			return host.at<float>(0, 0);
		}
		catch (...) {
			return EvaluateShiftNccSampled(img1, img2, width, height, sdx, sdy, 4);
		}
#else
		return EvaluateShiftNccSampled(img1, img2, width, height, sdx, sdy, 4);
#endif
	};

#pragma omp parallel
	{
		float tBest = best;
		float tBestObjective = bestObjective;
		int tBestDx = bestDx;
		int tBestDy = bestDy;

		#pragma omp for schedule(static) nowait
		for (int cy = minDy; cy <= maxDy; cy += coarseStep) {
			for (int cx = minDx; cx <= maxDx; cx += coarseStep) {
				float s = EvaluateLocalCoarse(cx, cy);
				float objective = s + ShiftSelectionBias(width, height, cx, cy);
				if (IsBetterCandidate(objective, s, cx, cy, tBestObjective, tBest, tBestDx, tBestDy)) {
					tBest = s;
					tBestObjective = objective;
					tBestDx = cx;
					tBestDy = cy;
				}
			}
		}

#pragma omp critical
		{
			if (IsBetterCandidate(tBestObjective, tBest, tBestDx, tBestDy, bestObjective, best, bestDx, bestDy)) {
				best = tBest;
				bestObjective = tBestObjective;
				bestDx = tBestDx;
				bestDy = tBestDy;
			}
		}
	}

	int refMinDx = bestDx - coarseStep;
	int refMaxDx = bestDx + coarseStep;
	int refMinDy = bestDy - coarseStep;
	int refMaxDy = bestDy + coarseStep;

	if (refMinDx < -width + 1) refMinDx = -width + 1;
	if (refMaxDx > width - 1) refMaxDx = width - 1;
	if (refMinDy < -height + 1) refMinDy = -height + 1;
	if (refMaxDy > height - 1) refMaxDy = height - 1;

#pragma omp parallel
	{
		float tBest = best;
		float tBestObjective = bestObjective;
		int tBestDx = bestDx;
		int tBestDy = bestDy;

		#pragma omp for schedule(static) nowait
		for (int cy = refMinDy; cy <= refMaxDy; cy++) {
			for (int cx = refMinDx; cx <= refMaxDx; cx++) {
				float s = EvaluateShiftNccSampled(img1, img2, width, height, cx, cy, 2);
				float objective = s + ShiftSelectionBias(width, height, cx, cy);
				if (IsBetterCandidate(objective, s, cx, cy, tBestObjective, tBest, tBestDx, tBestDy)) {
					tBest = s;
					tBestObjective = objective;
					tBestDx = cx;
					tBestDy = cy;
				}
			}
		}

#pragma omp critical
		{
			if (IsBetterCandidate(tBestObjective, tBest, tBestDx, tBestDy, bestObjective, best, bestDx, bestDy)) {
				best = tBest;
				bestObjective = tBestObjective;
				bestDx = tBestDx;
				bestDy = tBestDy;
			}
		}
	}

	if ((best >= 0.70f && best > baseScore + 0.02f) || (baseScore < 0.50f && best > baseScore)) {
		dx = bestDx;
		dy = bestDy;
	}
}

static void FallbackVerticalAnchoredSearch(BYTE* img1, BYTE* img2, int width, int height, int& dx, int& dy, float& angleDeg, float baseScore, float* bestCandidateScore, float* bestCandidateObjective, bool* acceptedCandidate)
{
	if (bestCandidateScore != nullptr)
		*bestCandidateScore = baseScore;
	if (bestCandidateObjective != nullptr)
		*bestCandidateObjective = baseScore + ShiftSelectionBias(width, height, dx, dy);
	if (acceptedCandidate != nullptr)
		*acceptedCandidate = false;

	if (img1 == nullptr || img2 == nullptr || width <= 1 || height <= 1)
		return;

	std::vector<BYTE>& grad1 = GetGradientImageCached(img1, width, height);
	std::vector<BYTE>& grad2 = GetGradientImageCached(img2, width, height);

	auto EvaluateCompositeScore = [&](int sdx, int sdy, float sang) -> float {
		float sIntensity = (sang == 0.0f)
			? EvaluateShiftNccSampled(img1, img2, width, height, sdx, sdy, 2)
			: EvaluateShiftNccSampledRotated(img1, img2, width, height, sdx, sdy, sang, 2);

		float sGradient = (sang == 0.0f)
			? EvaluateShiftNccSampled(grad1.data(), grad2.data(), width, height, sdx, sdy, 3)
			: EvaluateShiftNccSampledRotated(grad1.data(), grad2.data(), width, height, sdx, sdy, sang, 3);

		bool intensityValid = sIntensity > -1.5f;
		bool gradientValid = sGradient > -1.5f;
		if (intensityValid && gradientValid)
			return 0.30f * sIntensity + 0.70f * sGradient;
		if (gradientValid)
			return sGradient;
		if (intensityValid)
			return sIntensity;
		return -2.0f;
	};

	const int overlapPxCandidates[] = { 140, 180, 220, 260, 300, 340, 380, 460, 560 };
	const float overlapRatioCandidates[] = { 0.05f, 0.07f, 0.09f, 0.12f, 0.16f, 0.20f, 0.24f };
	const float angleCandidates[] = { -2.0f, 0.0f, 2.0f };
	int dxJitter = width / 220;
	if (dxJitter < 2) dxJitter = 2;
	if (dxJitter > 16) dxJitter = 16;

	std::vector<xy> candidates;
	candidates.reserve(120);
	for (int oi = 0; oi < 9; oi++) {
		int overlapPx = overlapPxCandidates[oi];
		int shiftY = height - overlapPx;
		if (shiftY >= 1 && shiftY < height) {
			for (int j = -1; j <= 1; j++) {
				candidates.push_back({ j * dxJitter, shiftY });
				candidates.push_back({ j * dxJitter, -shiftY });
			}
		}
	}
	for (int oi = 0; oi < 7; oi++) {
		float overlap = overlapRatioCandidates[oi];
		int shiftY = height - (int)(overlap * (float)height);
		if (shiftY >= 1 && shiftY < height) {
			for (int j = -1; j <= 1; j++) {
				candidates.push_back({ j * dxJitter, shiftY });
				candidates.push_back({ j * dxJitter, -shiftY });
			}
		}
	}
	auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(GetRotatedFallbackBudgetMs());
	std::atomic<bool> stopSearch(false);

	int bestDx = dx;
	int bestDy = dy;
	float bestAng = angleDeg;
	float bestScore = EvaluateCompositeScore(dx, dy, angleDeg);
	if (bestScore <= -1.5f)
		bestScore = baseScore;
	float bestObjective = baseScore + ShiftSelectionBias(width, height, dx, dy) + EdgeOverlapTargetBias(width, height, dx, dy);

	auto IsBetterCandidate = [](float objA, float scoreA, int dxA, int dyA, float angA,
		float objB, float scoreB, int dxB, int dyB, float angB) -> bool {
		if (objA > objB) return true;
		if (objA < objB) return false;
		if (scoreA > scoreB) return true;
		if (scoreA < scoreB) return false;
		float absAngA = (float)fabs(angA);
		float absAngB = (float)fabs(angB);
		if (absAngA < absAngB) return true;
		if (absAngA > absAngB) return false;
		int magA = abs(dxA) + abs(dyA);
		int magB = abs(dxB) + abs(dyB);
		if (magA < magB) return true;
		if (magA > magB) return false;
		if (dyA < dyB) return true;
		if (dyA > dyB) return false;
		return dxA < dxB;
	};

	#pragma omp parallel
	{
		int tBestDx = bestDx;
		int tBestDy = bestDy;
		float tBestAng = bestAng;
		float tBestScore = bestScore;
		float tBestObjective = bestObjective;
		int tIter = 0;

		#pragma omp for schedule(static) nowait
		for (int ci = 0; ci < (int)candidates.size(); ci++) {
			if (stopSearch.load())
				continue;
			for (int ai = 0; ai < 3; ai++) {
				if (stopSearch.load())
					break;
				int dxCand = candidates[(size_t)ci].x;
				int dyCand = candidates[(size_t)ci].y;
				if ((++tIter & 31) == 0 && std::chrono::steady_clock::now() >= deadline) {
					stopSearch.store(true);
					break;
				}
					float candAng = angleCandidates[ai];
					float s = EvaluateCompositeScore(dxCand, dyCand, candAng);
					// Strongly favor vertical movement here; horizontal motion is treated as a fallback-only side effect.
					float verticalBias = 0.30f * ((float)abs(dyCand) / (float)(height > 0 ? height : 1));
					float horizontalPenalty = 0.45f * ((float)abs(dxCand) / (float)(width > 0 ? width : 1));
					float objective = s + verticalBias - horizontalPenalty - (float)fabs(candAng) * 0.012f;
					if (IsBetterCandidate(objective, s, dxCand, dyCand, candAng, tBestObjective, tBestScore, tBestDx, tBestDy, tBestAng)) {
						tBestObjective = objective;
						tBestScore = s;
						tBestDx = dxCand;
						tBestDy = dyCand;
						tBestAng = candAng;
					}
				}
			}

		#pragma omp critical
		{
			if (IsBetterCandidate(tBestObjective, tBestScore, tBestDx, tBestDy, tBestAng, bestObjective, bestScore, bestDx, bestDy, bestAng)) {
				bestObjective = tBestObjective;
				bestScore = tBestScore;
				bestDx = tBestDx;
				bestDy = tBestDy;
				bestAng = tBestAng;
			}
		}
	}

	const bool accept = (bestObjective > baseScore + 0.005f) && (bestScore > -1.0f);
	if (accept) {
		dx = bestDx;
		dy = bestDy;
		angleDeg = bestAng;
	}

	if (bestCandidateScore != nullptr)
		*bestCandidateScore = bestScore;
	if (bestCandidateObjective != nullptr)
		*bestCandidateObjective = bestObjective;
	if (acceptedCandidate != nullptr)
		*acceptedCandidate = accept;
}

static void FallbackHorizontalAnchoredSearch(BYTE* img1, BYTE* img2, int width, int height, int& dx, int& dy, float& angleDeg, float baseScore, float* bestCandidateScore, float* bestCandidateObjective, bool* acceptedCandidate)
{
	if (bestCandidateScore != nullptr)
		*bestCandidateScore = baseScore;
	if (bestCandidateObjective != nullptr)
		*bestCandidateObjective = baseScore + ShiftSelectionBias(width, height, dx, dy);
	if (acceptedCandidate != nullptr)
		*acceptedCandidate = false;

	if (img1 == nullptr || img2 == nullptr || width <= 1 || height <= 1)
		return;

	std::vector<BYTE>& grad1 = GetGradientImageCached(img1, width, height);
	std::vector<BYTE>& grad2 = GetGradientImageCached(img2, width, height);

	auto EvaluateCompositeScore = [&](int sdx, int sdy, float sang) -> float {
		float sIntensity = (sang == 0.0f)
			? EvaluateShiftNccSampled(img1, img2, width, height, sdx, sdy, 2)
			: EvaluateShiftNccSampledRotated(img1, img2, width, height, sdx, sdy, sang, 2);

		float sGradient = (sang == 0.0f)
			? EvaluateShiftNccSampled(grad1.data(), grad2.data(), width, height, sdx, sdy, 3)
			: EvaluateShiftNccSampledRotated(grad1.data(), grad2.data(), width, height, sdx, sdy, sang, 3);

		bool intensityValid = sIntensity > -1.5f;
		bool gradientValid = sGradient > -1.5f;
		if (intensityValid && gradientValid)
			return 0.30f * sIntensity + 0.70f * sGradient;
		if (gradientValid)
			return sGradient;
		if (intensityValid)
			return sIntensity;
		return -2.0f;
	};

	const int overlapPxCandidates[] = { 140, 180, 220, 260, 300, 340, 380, 460, 560 };
	const float overlapRatioCandidates[] = { 0.05f, 0.07f, 0.09f, 0.12f, 0.16f, 0.20f, 0.24f };
	const float angleCandidates[] = { -2.0f, 0.0f, 2.0f };
	int dyJitter = height / 220;
	if (dyJitter < 2) dyJitter = 2;
	if (dyJitter > 16) dyJitter = 16;

	std::vector<xy> candidates;
	candidates.reserve(120);
	for (int oi = 0; oi < 9; oi++) {
		int overlapPx = overlapPxCandidates[oi];
		int shiftX = width - overlapPx;
		if (shiftX >= 1 && shiftX < width) {
			for (int j = -1; j <= 1; j++) {
				candidates.push_back({ shiftX, j * dyJitter });
				candidates.push_back({ -shiftX, j * dyJitter });
			}
		}
	}
	for (int oi = 0; oi < 7; oi++) {
		float overlap = overlapRatioCandidates[oi];
		int shiftX = width - (int)(overlap * (float)width);
		if (shiftX >= 1 && shiftX < width) {
			for (int j = -1; j <= 1; j++) {
				candidates.push_back({ shiftX, j * dyJitter });
				candidates.push_back({ -shiftX, j * dyJitter });
			}
		}
	}
	auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(GetRotatedFallbackBudgetMs());
	std::atomic<bool> stopSearch(false);

	int bestDx = dx;
	int bestDy = dy;
	float bestAng = angleDeg;
	float bestScore = EvaluateCompositeScore(dx, dy, angleDeg);
	if (bestScore <= -1.5f)
		bestScore = baseScore;
	float bestObjective = baseScore + ShiftSelectionBias(width, height, dx, dy) + EdgeOverlapTargetBias(width, height, dx, dy);

	auto IsBetterCandidate = [](float objA, float scoreA, int dxA, int dyA, float angA,
		float objB, float scoreB, int dxB, int dyB, float angB) -> bool {
		if (objA > objB) return true;
		if (objA < objB) return false;
		if (scoreA > scoreB) return true;
		if (scoreA < scoreB) return false;
		float absAngA = (float)fabs(angA);
		float absAngB = (float)fabs(angB);
		if (absAngA < absAngB) return true;
		if (absAngA > absAngB) return false;
		int magA = abs(dxA) + abs(dyA);
		int magB = abs(dxB) + abs(dyB);
		if (magA < magB) return true;
		if (magA > magB) return false;
		if (dxA < dxB) return true;
		if (dxA > dxB) return false;
		return dyA < dyB;
	};

	#pragma omp parallel
	{
		int tBestDx = bestDx;
		int tBestDy = bestDy;
		float tBestAng = bestAng;
		float tBestScore = bestScore;
		float tBestObjective = bestObjective;
		int tIter = 0;

		#pragma omp for schedule(static) nowait
		for (int ci = 0; ci < (int)candidates.size(); ci++) {
			if (stopSearch.load())
				continue;
			for (int ai = 0; ai < 3; ai++) {
				if (stopSearch.load())
					break;
				int dxCand = candidates[(size_t)ci].x;
				int dyCand = candidates[(size_t)ci].y;
				if ((++tIter & 31) == 0 && std::chrono::steady_clock::now() >= deadline) {
					stopSearch.store(true);
					break;
				}
					float candAng = angleCandidates[ai];
					float s = EvaluateCompositeScore(dxCand, dyCand, candAng);
					float horizontalBias = 0.30f * ((float)abs(dxCand) / (float)(width > 0 ? width : 1));
					float verticalPenalty = 0.45f * ((float)abs(dyCand) / (float)(height > 0 ? height : 1));
					float objective = s + horizontalBias - verticalPenalty - (float)fabs(candAng) * 0.012f;
					if (IsBetterCandidate(objective, s, dxCand, dyCand, candAng, tBestObjective, tBestScore, tBestDx, tBestDy, tBestAng)) {
						tBestObjective = objective;
						tBestScore = s;
						tBestDx = dxCand;
						tBestDy = dyCand;
						tBestAng = candAng;
					}
				}
			}

		#pragma omp critical
		{
			if (IsBetterCandidate(tBestObjective, tBestScore, tBestDx, tBestDy, tBestAng, bestObjective, bestScore, bestDx, bestDy, bestAng)) {
				bestObjective = tBestObjective;
				bestScore = tBestScore;
				bestDx = tBestDx;
				bestDy = tBestDy;
				bestAng = tBestAng;
			}
		}
	}

	const bool accept = (bestObjective > baseScore + 0.005f) && (bestScore > -1.0f);
	if (accept) {
		dx = bestDx;
		dy = bestDy;
		angleDeg = bestAng;
	}

	if (bestCandidateScore != nullptr)
		*bestCandidateScore = bestScore;
	if (bestCandidateObjective != nullptr)
		*bestCandidateObjective = bestObjective;
	if (acceptedCandidate != nullptr)
		*acceptedCandidate = accept;
}

static void FallbackEdgeAnchoredSearch(BYTE* img1, BYTE* img2, int width, int height, int& dx, int& dy, float& angleDeg, float baseScore, float* bestCandidateScore, float* bestCandidateObjective, bool* acceptedCandidate)
{
	if (bestCandidateScore != nullptr)
		*bestCandidateScore = baseScore;
	if (bestCandidateObjective != nullptr)
		*bestCandidateObjective = baseScore + ShiftSelectionBias(width, height, dx, dy) + EdgeOverlapTargetBias(width, height, dx, dy);
	if (acceptedCandidate != nullptr)
		*acceptedCandidate = false;

	if (img1 == nullptr || img2 == nullptr || width <= 1 || height <= 1)
		return;

	std::vector<BYTE>& grad1 = GetGradientImageCached(img1, width, height);
	std::vector<BYTE>& grad2 = GetGradientImageCached(img2, width, height);

	auto EvaluateCompositeScore = [&](int sdx, int sdy, float sang) -> float {
		float sIntensity = (sang == 0.0f)
			? EvaluateShiftNccSampled(img1, img2, width, height, sdx, sdy, 2)
			: EvaluateShiftNccSampledRotated(img1, img2, width, height, sdx, sdy, sang, 2);

		float sGradient = (sang == 0.0f)
			? EvaluateShiftNccSampled(grad1.data(), grad2.data(), width, height, sdx, sdy, 3)
			: EvaluateShiftNccSampledRotated(grad1.data(), grad2.data(), width, height, sdx, sdy, sang, 3);

		bool intensityValid = sIntensity > -1.5f;
		bool gradientValid = sGradient > -1.5f;
		if (intensityValid && gradientValid)
			return 0.35f * sIntensity + 0.65f * sGradient;
		if (gradientValid)
			return sGradient;
		if (intensityValid)
			return sIntensity;
		return -2.0f;
	};

	const int overlapPxCandidates[] = { 140, 180, 220, 260, 300, 340, 380, 460, 560 };
	const float overlapRatioCandidates[] = { 0.05f, 0.07f, 0.09f, 0.12f, 0.16f, 0.20f, 0.24f };
	const float angleCandidates[] = { -5.0f, -3.0f, -1.0f, 0.0f, 1.0f, 3.0f, 5.0f };

	int bestDx = dx;
	int bestDy = dy;
	float bestAng = angleDeg;
	float bestScore = EvaluateCompositeScore(dx, dy, angleDeg);
	if (bestScore <= -1.5f)
		bestScore = baseScore;
	float bestObjective = baseScore + ShiftSelectionBias(width, height, dx, dy) + EdgeOverlapTargetBias(width, height, dx, dy);
	const float baseObjective = bestObjective;

	int orthoJitterX = width / 300;
	int orthoJitterY = height / 300;
	if (orthoJitterX < 2) orthoJitterX = 2;
	if (orthoJitterY < 2) orthoJitterY = 2;
	if (orthoJitterX > 18) orthoJitterX = 18;
	if (orthoJitterY > 18) orthoJitterY = 18;

	std::vector<xy> candidates;
	candidates.reserve(120);
	for (int oi = 0; oi < 9; oi++) {
		int overlapPx = overlapPxCandidates[oi];
		int shiftX = width - overlapPx;
		int shiftY = height - overlapPx;

		if (shiftX >= 1 && shiftX < width) {
			for (int j = -1; j <= 1; j++) {
				candidates.push_back({ shiftX, j * orthoJitterY });
				candidates.push_back({ -shiftX, j * orthoJitterY });
			}
		}

		if (shiftY >= 1 && shiftY < height) {
			for (int j = -1; j <= 1; j++) {
				candidates.push_back({ j * orthoJitterX, shiftY });
				candidates.push_back({ j * orthoJitterX, -shiftY });
			}
		}
	}

	for (int oi = 0; oi < 7; oi++) {
		float overlap = overlapRatioCandidates[oi];
		int shiftX = width - (int)(overlap * (float)width);
		int shiftY = height - (int)(overlap * (float)height);

		if (shiftX >= 1 && shiftX < width) {
			for (int j = -1; j <= 1; j++) {
				candidates.push_back({ shiftX, j * orthoJitterY });
				candidates.push_back({ -shiftX, j * orthoJitterY });
			}
		}

		if (shiftY >= 1 && shiftY < height) {
			for (int j = -1; j <= 1; j++) {
				candidates.push_back({ j * orthoJitterX, shiftY });
				candidates.push_back({ j * orthoJitterX, -shiftY });
			}
		}
	}
	auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(GetRotatedFallbackBudgetMs());
	std::atomic<bool> stopSearch(false);

	auto IsBetterCandidate = [](float objA, float scoreA, int dxA, int dyA, float angA,
		float objB, float scoreB, int dxB, int dyB, float angB) -> bool {
		if (objA > objB) return true;
		if (objA < objB) return false;
		if (scoreA > scoreB) return true;
		if (scoreA < scoreB) return false;
		float absAngA = (float)fabs(angA);
		float absAngB = (float)fabs(angB);
		if (absAngA < absAngB) return true;
		if (absAngA > absAngB) return false;
		int magA = abs(dxA) + abs(dyA);
		int magB = abs(dxB) + abs(dyB);
		if (magA < magB) return true;
		if (magA > magB) return false;
		if (dyA < dyB) return true;
		if (dyA > dyB) return false;
		return dxA < dxB;
	};

#pragma omp parallel
	{
		int tBestDx = bestDx;
		int tBestDy = bestDy;
		float tBestAng = bestAng;
		float tBestScore = bestScore;
		float tBestObjective = bestObjective;
		int tIter = 0;

#pragma omp for schedule(static) nowait
		for (int ci = 0; ci < (int)candidates.size(); ci++) {
			if (stopSearch.load())
				continue;
			for (int ai = 0; ai < 7; ai++) {
				if (stopSearch.load())
					break;
				int candDx = candidates[(size_t)ci].x;
				int candDy = candidates[(size_t)ci].y;
				if ((++tIter & 31) == 0 && std::chrono::steady_clock::now() >= deadline) {
					stopSearch.store(true);
					break;
				}
				float candAng = angleCandidates[ai];
				float s = EvaluateCompositeScore(candDx, candDy, candAng);
				float objective = s + ShiftSelectionBias(width, height, candDx, candDy) + EdgeOverlapTargetBias(width, height, candDx, candDy) - (float)fabs(candAng) * 0.012f + 0.06f;
				if (IsBetterCandidate(objective, s, candDx, candDy, candAng, tBestObjective, tBestScore, tBestDx, tBestDy, tBestAng)) {
					tBestObjective = objective;
					tBestScore = s;
					tBestDx = candDx;
					tBestDy = candDy;
					tBestAng = candAng;
				}
			}
		}

#pragma omp critical
		{
			if (IsBetterCandidate(tBestObjective, tBestScore, tBestDx, tBestDy, tBestAng, bestObjective, bestScore, bestDx, bestDy, bestAng)) {
				bestObjective = tBestObjective;
				bestScore = tBestScore;
				bestDx = tBestDx;
				bestDy = tBestDy;
				bestAng = tBestAng;
			}
		}
	}

   // Hard overlap constraint: prefer primary overlaps in a reasonable range unless candidate is significantly better
	int primaryDimCurrent = (abs(dx) >= abs(dy)) ? width : height;
	int MIN_OVERLAP_PX = primaryDimCurrent / 18;
	if (MIN_OVERLAP_PX < 40) MIN_OVERLAP_PX = 40;
	int MAX_OVERLAP_PX = (int)(0.55f * (float)primaryDimCurrent);
	if (MAX_OVERLAP_PX > primaryDimCurrent - 10) MAX_OVERLAP_PX = primaryDimCurrent - 10;
	const float REQUIRED_ADVANTAGE = 0.10f; // candidate must beat base by this margin if overlap out of range
	const bool veryLowConfidence = baseScore < 0.35f;
	bool accept = false;
	if (veryLowConfidence) {
		// In very low-confidence regimes, rely on geometry/objective bias to escape phase-correlation local minima.
		accept = (bestObjective > baseObjective + 0.005f);
	}
	else {
		accept = (bestObjective > baseObjective + 0.02f) && (bestScore > 0.26f);
	}

	// compute primary overlap of chosen candidate
	int absDxBest = abs(bestDx);
	int absDyBest = abs(bestDy);
	bool horizontalNeighborBest = absDxBest >= absDyBest;
	int primaryOverlapBest = horizontalNeighborBest ? (width - absDxBest) : (height - absDyBest);
    // if primary overlap is outside acceptable range, require significant advantage
	if (accept && GetHardOverlapCheck()) {
		if (primaryOverlapBest < MIN_OVERLAP_PX || primaryOverlapBest > MAX_OVERLAP_PX) {
			if (!(bestObjective > baseObjective + REQUIRED_ADVANTAGE)) {
				// reject candidate despite earlier acceptance
				accept = false;
			}
		}
	}

	if (accept) {
		dx = bestDx;
		dy = bestDy;
		angleDeg = bestAng;
	}

	if (bestCandidateScore != nullptr)
		*bestCandidateScore = bestScore;
	if (bestCandidateObjective != nullptr)
		*bestCandidateObjective = bestObjective;
	if (acceptedCandidate != nullptr)
		*acceptedCandidate = accept;
}

float PhaseShiftConfidence(BYTE* img1, BYTE* img2, int width, int height, xy* pocDot)
{
	if (img1 == nullptr || img2 == nullptr || pocDot == nullptr || width <= 1 || height <= 1)
		return -1.0f;

	int dx = pocDot->x;
	int dy = pocDot->y;

	if (dx > width / 2)
		dx -= width;
	if (dy > height / 2)
		dy -= height;

	return EvaluateShiftNccSampled(img1, img2, width, height, dx, dy, 2);
}

static void RefineShiftRotationLocal(BYTE* img1, BYTE* img2, int width, int height, int& dx, int& dy, float& angleDeg)
{
	if (img1 == nullptr || img2 == nullptr || width <= 1 || height <= 1)
		return;

	std::vector<BYTE>& grad1 = GetGradientImageCached(img1, width, height);
	std::vector<BYTE>& grad2 = GetGradientImageCached(img2, width, height);

	auto EvaluateCompositeScore = [&](int sdx, int sdy, float sang) -> float {
		float sIntensity = (sang == 0.0f)
			? EvaluateShiftNccSampled(img1, img2, width, height, sdx, sdy, 2)
			: EvaluateShiftNccSampledRotated(img1, img2, width, height, sdx, sdy, sang, 2);
		float sGradient = (sang == 0.0f)
			? EvaluateShiftNccSampled(grad1.data(), grad2.data(), width, height, sdx, sdy, 3)
			: EvaluateShiftNccSampledRotated(grad1.data(), grad2.data(), width, height, sdx, sdy, sang, 3);

		bool intensityValid = sIntensity > -1.5f;
		bool gradientValid = sGradient > -1.5f;
		if (intensityValid && gradientValid)
			return 0.35f * sIntensity + 0.65f * sGradient;
		if (gradientValid)
			return sGradient;
		if (intensityValid)
			return sIntensity;
		return -2.0f;
	};

	auto IsBetterCandidate = [](float objA, float scoreA, int dxA, int dyA, float angA,
		float objB, float scoreB, int dxB, int dyB, float angB) -> bool {
		if (objA > objB) return true;
		if (objA < objB) return false;
		if (scoreA > scoreB) return true;
		if (scoreA < scoreB) return false;
		float absAngA = (float)fabs(angA);
		float absAngB = (float)fabs(angB);
		if (absAngA < absAngB) return true;
		if (absAngA > absAngB) return false;
		int magA = abs(dxA) + abs(dyA);
		int magB = abs(dxB) + abs(dyB);
		if (magA < magB) return true;
		if (magA > magB) return false;
		if (dyA < dyB) return true;
		if (dyA > dyB) return false;
		return dxA < dxB;
	};

	int bestDx = dx;
	int bestDy = dy;
	float bestAng = angleDeg;
	float bestScore = EvaluateCompositeScore(bestDx, bestDy, bestAng);
	float bestObjective = bestScore + ShiftSelectionBias(width, height, bestDx, bestDy) + EdgeOverlapTargetBias(width, height, bestDx, bestDy) - (float)fabs(bestAng) * 0.01f;

	const int dxRange[2] = { 6, 3 };
	const int dyRange[2] = { 6, 3 };
	const float angRange[2] = { 1.0f, 0.45f };
	const float angStep[2] = { 0.25f, 0.10f };
	int refineBudgetMs = (GetPhaseCorrelationBackend() == PhaseCorrelationBackendGpu) ? 450 : 700;
	auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(refineBudgetMs);

	for (int pass = 0; pass < 2; pass++) {
		int centerDx = bestDx;
		int centerDy = bestDy;
		float centerAng = bestAng;
		for (int cy = centerDy - dyRange[pass]; cy <= centerDy + dyRange[pass]; cy++) {
			if (std::chrono::steady_clock::now() >= deadline)
				return;
			if (cy < -height + 1 || cy > height - 1)
				continue;
			for (int cx = centerDx - dxRange[pass]; cx <= centerDx + dxRange[pass]; cx++) {
				if (cx < -width + 1 || cx > width - 1)
					continue;
				for (float ca = centerAng - angRange[pass]; ca <= centerAng + angRange[pass] + 1e-6f; ca += angStep[pass]) {
					if (std::chrono::steady_clock::now() >= deadline)
						return;
					float candAng = ca;
					if (candAng > 6.0f) candAng = 6.0f;
					if (candAng < -6.0f) candAng = -6.0f;
					float s = EvaluateCompositeScore(cx, cy, candAng);
					float objective = s + ShiftSelectionBias(width, height, cx, cy) + EdgeOverlapTargetBias(width, height, cx, cy) - (float)fabs(candAng) * 0.01f;
					if (IsBetterCandidate(objective, s, cx, cy, candAng, bestObjective, bestScore, bestDx, bestDy, bestAng)) {
						bestObjective = objective;
						bestScore = s;
						bestDx = cx;
						bestDy = cy;
						bestAng = candAng;
					}
				}
			}
		}
	}

	dx = bestDx;
	dy = bestDy;
	angleDeg = bestAng;
}

static void RefineAxisDominantTranslation(BYTE* img1, BYTE* img2, int width, int height, int& dx, int& dy, float& angleDeg)
{
	if (img1 == nullptr || img2 == nullptr || width <= 1 || height <= 1)
		return;

	std::vector<BYTE>& grad1 = GetGradientImageCached(img1, width, height);
	std::vector<BYTE>& grad2 = GetGradientImageCached(img2, width, height);

	auto EvaluateComposite = [&](int sdx, int sdy) -> float {
		float si = EvaluateShiftNccSampled(img1, img2, width, height, sdx, sdy, 2);
		float sg = EvaluateShiftNccSampled(grad1.data(), grad2.data(), width, height, sdx, sdy, 3);
		bool vi = si > -1.5f;
		bool vg = sg > -1.5f;
		if (vi && vg) return 0.35f * si + 0.65f * sg;
		if (vg) return sg;
		if (vi) return si;
		return -2.0f;
	};

	int bestDx = dx;
	int bestDy = dy;
	const int originDx = dx;
	const int originDy = dy;
	float best = EvaluateComposite(dx, dy);
	float bestObjective = best;

	const bool verticalDominant = abs(dy) >= abs(dx);
	const int primaryRange = verticalDominant ? 14 : 10;
	const int crossRange = verticalDominant ? 3 : 2;
	int axisBudgetMs = (GetPhaseCorrelationBackend() == PhaseCorrelationBackendGpu) ? 180 : 260;
	auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(axisBudgetMs);

	for (int p = -primaryRange; p <= primaryRange; p++) {
		if (std::chrono::steady_clock::now() >= deadline)
			break;
		for (int c = -crossRange; c <= crossRange; c++) {
			int candDx = verticalDominant ? (dx + c) : (dx + p);
			int candDy = verticalDominant ? (dy + p) : (dy + c);
			if (candDx < -width + 1 || candDx > width - 1 || candDy < -height + 1 || candDy > height - 1)
				continue;
			float s = EvaluateComposite(candDx, candDy);
			const int move = abs(candDx - originDx) + abs(candDy - originDy);
			const float movePenalty = 0.010f * (float)move;
			float objective = s - movePenalty;
			if (objective > bestObjective || (objective == bestObjective && move < (abs(bestDx - originDx) + abs(bestDy - originDy)))) {
				best = s;
				bestObjective = objective;
				bestDx = candDx;
				bestDy = candDy;
			}
		}
	}

	if (best > -1.2f && bestObjective > (EvaluateComposite(originDx, originDy) + 0.003f)) {
		dx = bestDx;
		dy = bestDy;
		// For translation fallback, suppress tiny rotational noise that causes edge ghosting.
		if (fabs(angleDeg) <= 2.2f)
			angleDeg = 0.0f;
	}
}

static float EvaluateVerticalSeamEdgeScore(BYTE* img1, BYTE* img2, int width, int height, int dx, int dy, int sampleStep)
{
	if (img1 == nullptr || img2 == nullptr || width <= 2 || height <= 2)
		return -2.0f;

	if (sampleStep < 1)
		sampleStep = 1;

	const int xStart = (dx > 0) ? dx : 0;
	const int yStart = (dy > 0) ? dy : 0;
	const int xEnd = (width + dx - 1 < width - 1) ? (width + dx - 1) : (width - 1);
	const int yEnd = (height + dy - 1 < height - 1) ? (height + dy - 1) : (height - 1);

	if (xStart + 1 >= xEnd || yStart + 1 >= yEnd)
		return -2.0f;

	const int overlapW = xEnd - xStart + 1;
	const int overlapH = yEnd - yStart + 1;
	if (overlapW < width / 14 || overlapH < height / 16)
		return -2.0f;

	// Restrict scoring to a seam-centered vertical band and prioritize vertical-edge alignment.
	int seamBandY0 = yStart + overlapH / 4;
	int seamBandY1 = yEnd - overlapH / 4;
	if (seamBandY1 - seamBandY0 < 24) {
		seamBandY0 = yStart;
		seamBandY1 = yEnd;
	}

	double wAbsDiff = 0.0;
	double wTotal = 0.0;
	double s1 = 0.0, s2 = 0.0, s11 = 0.0, s22 = 0.0, s12 = 0.0;
	int n = 0;

	for (int y = seamBandY0; y <= seamBandY1; y += sampleStep) {
		const int y2 = y - dy;
		const int row1 = y * width;
		const int row2 = y2 * width;
		for (int x = xStart + 1; x <= xEnd - 1; x += sampleStep) {
			const int x2 = x - dx;
			if (x2 <= 0 || x2 >= width - 1)
				continue;

			const double a = (double)img1[row1 + x];
			const double b = (double)img2[row2 + x2];

			const int gx1 = abs((int)img1[row1 + (x + 1)] - (int)img1[row1 + (x - 1)]);
			const int gx2 = abs((int)img2[row2 + (x2 + 1)] - (int)img2[row2 + (x2 - 1)]);
			double w = (double)((gx1 > gx2) ? gx1 : gx2);
			if (w < 8.0)
				continue;

			wAbsDiff += w * fabs(a - b);
			wTotal += w;

			s1 += a;
			s2 += b;
			s11 += a * a;
			s22 += b * b;
			s12 += a * b;
			n++;
		}
	}

	if (n < 128 || wTotal <= 1e-9)
		return -2.0f;

	const double nf = (double)n;
	const double num = s12 - (s1 * s2) / nf;
	const double den1 = s11 - (s1 * s1) / nf;
	const double den2 = s22 - (s2 * s2) / nf;
	const double den = sqrt(den1 * den2);
	if (den <= 1e-12)
		return -2.0f;

	const double ncc = num / den;
	const double edgeAgreement = 1.0 - (wAbsDiff / (wTotal * 255.0));
	const double clampedEdgeAgreement = (edgeAgreement < -1.0) ? -1.0 : ((edgeAgreement > 1.0) ? 1.0 : edgeAgreement);
	return (float)(0.40 * ncc + 0.60 * clampedEdgeAgreement);
}

static void RefineCrossAxisForVerticalSeam(BYTE* img1, BYTE* img2, int width, int height, int& dx, int& dy)
{
	if (img1 == nullptr || img2 == nullptr || width <= 1 || height <= 1)
		return;
	if (abs(dy) < abs(dx) * 2)
		return;

	std::vector<BYTE>& grad1 = GetGradientImageCached(img1, width, height);
	std::vector<BYTE>& grad2 = GetGradientImageCached(img2, width, height);

	auto Score = [&](int sdx) -> float {
		float si = EvaluateShiftNccSampled(img1, img2, width, height, sdx, dy, 2);
		float sg = EvaluateShiftNccSampled(grad1.data(), grad2.data(), width, height, sdx, dy, 3);
		float seamEdge = EvaluateVerticalSeamEdgeScore(img1, img2, width, height, sdx, dy, 3);

		if (si > -1.5f && sg > -1.5f && seamEdge > -1.5f)
			return 0.20f * si + 0.30f * sg + 0.50f * seamEdge;
		if (sg > -1.5f && seamEdge > -1.5f)
			return 0.45f * sg + 0.55f * seamEdge;
		if (si > -1.5f && seamEdge > -1.5f)
			return 0.45f * si + 0.55f * seamEdge;
		if (seamEdge > -1.5f)
			return seamEdge;
		if (sg > -1.5f)
			return sg;
		if (si > -1.5f)
			return si;
		return -2.0f;
	};

	const int originDx = dx;
	int bestDx = dx;
	float best = Score(dx);
	float bestObjective = best;
	int crossAxisBudgetMs = (GetPhaseCorrelationBackend() == PhaseCorrelationBackendGpu) ? 150 : 220;
	auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(crossAxisBudgetMs);

	for (int off = -18; off <= 18; off++) {
		if (std::chrono::steady_clock::now() >= deadline)
			break;
		int candDx = originDx + off;
		if (candDx < -width + 1 || candDx > width - 1)
			continue;
		float s = Score(candDx);
		// Keep cross-axis correction conservative; large dx jumps are often score noise on repetitive PCB texture.
		const float movePenalty = 0.0125f * (float)abs(candDx - originDx);
		float objective = s - movePenalty;
		if (objective > bestObjective || (objective == bestObjective && abs(candDx - originDx) < abs(bestDx - originDx))) {
			best = s;
			bestObjective = objective;
			bestDx = candDx;
		}
	}

	// Only apply if the seam objective improves meaningfully versus staying at the original dx.
	if (best > -1.2f && bestObjective > (Score(originDx) + 0.004f))
		dx = bestDx;
}

static void EnforceVerticalSeamDxEnsemble(BYTE* img1, BYTE* img2, int width, int height, int& dx, int dy)
{
	if (img1 == nullptr || img2 == nullptr || width <= 2 || height <= 2)
		return;
	if (abs(dy) < height / 3)
		return;

	std::vector<BYTE>& grad1 = GetGradientImageCached(img1, width, height);
	std::vector<BYTE>& grad2 = GetGradientImageCached(img2, width, height);

	auto Score = [&](int sdx) -> float {
		float seam = EvaluateVerticalSeamEdgeScore(img1, img2, width, height, sdx, dy, 3);
		float si = EvaluateShiftNccSampled(img1, img2, width, height, sdx, dy, 2);
		float sg = EvaluateShiftNccSampled(grad1.data(), grad2.data(), width, height, sdx, dy, 3);

		float base = -2.0f;
		if (seam > -1.5f && si > -1.5f && sg > -1.5f)
			base = 0.50f * seam + 0.25f * si + 0.25f * sg;
		else if (seam > -1.5f && sg > -1.5f)
			base = 0.65f * seam + 0.35f * sg;
		else if (seam > -1.5f && si > -1.5f)
			base = 0.65f * seam + 0.35f * si;
		else if (seam > -1.5f)
			base = seam;
		else if (sg > -1.5f)
			base = sg;
		else if (si > -1.5f)
			base = si;

		if (base <= -1.5f)
			return -2.0f;

		// In low-confidence vertical stitching, avoid being trapped at dx ~= 0 on repetitive textures.
		float nonZeroBias = 0.0f;
		int adx = abs(sdx);
		if (adx >= 6 && adx <= 20)
			nonZeroBias = 0.006f;
		return base + nonZeroBias;
	};

	const int originDx = dx;
	float originScore = Score(originDx);
	int bestDx = originDx;
	float bestScore = originScore;

	int ensembleBudgetMs = (GetPhaseCorrelationBackend() == PhaseCorrelationBackendGpu) ? 150 : 220;
	auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ensembleBudgetMs);
	for (int candDx = -24; candDx <= 24; candDx += 2) {
		if (std::chrono::steady_clock::now() >= deadline)
			break;
		if (candDx < -width + 1 || candDx > width - 1)
			continue;
		float s = Score(candDx);
		if (s > bestScore || (s == bestScore && abs(candDx) > abs(bestDx))) {
			bestScore = s;
			bestDx = candDx;
		}
	}

	if (bestScore > originScore + 0.002f || (abs(originDx) <= 3 && abs(bestDx) >= 6 && bestScore > originScore - 0.003f))
		dx = bestDx;
}

static void ResolveVerticalDxSignLowConfidence(BYTE* img1, BYTE* img2, int width, int height, int& dx, int dy)
{
	if (img1 == nullptr || img2 == nullptr || width <= 2 || height <= 2)
		return;
	if (abs(dy) < height / 3)
		return;
	int adx = abs(dx);
	if (adx < 4)
		return;

	std::vector<BYTE>& grad1 = GetGradientImageCached(img1, width, height);
	std::vector<BYTE>& grad2 = GetGradientImageCached(img2, width, height);

	auto Score = [&](int sdx) -> float {
		float seam = EvaluateVerticalSeamEdgeScore(img1, img2, width, height, sdx, dy, 3);
		float si = EvaluateShiftNccSampled(img1, img2, width, height, sdx, dy, 2);
		float sg = EvaluateShiftNccSampled(grad1.data(), grad2.data(), width, height, sdx, dy, 3);
		if (seam > -1.5f && si > -1.5f && sg > -1.5f)
			return 0.55f * seam + 0.25f * si + 0.20f * sg;
		if (seam > -1.5f && sg > -1.5f)
			return 0.70f * seam + 0.30f * sg;
		if (seam > -1.5f && si > -1.5f)
			return 0.70f * seam + 0.30f * si;
		if (seam > -1.5f)
			return seam;
		if (sg > -1.5f)
			return sg;
		if (si > -1.5f)
			return si;
		return -2.0f;
	};

	float sPos = Score(adx);
	float sNeg = Score(-adx);
	if (sPos <= -1.5f && sNeg <= -1.5f)
		return;
	if (sPos > sNeg + 0.004f) {
		dx = adx;
		return;
	}
	if (sNeg > sPos + 0.004f) {
		dx = -adx;
		return;
	}

	// Ambiguous sign: prefer positive dx for stable corner selection in vertical low-confidence runs.
	dx = adx;
}

//float Correlation(BYTE* img1, BYTE* img2, int width, int height, int zoneWidth, int zoneHeight, int start1H, int start1W, int start2H, int start2W, BYTE v) {
//
//	int newR1, newC1, newR2, newC2;
//	unsigned int total1 = 0, total2 = 0;
//	double total = 0.0F, totalSqr1 = 0.0F, totalSqr2 = 0.0F;
//	double mean1, mean2;
//
//
//	for (int r = 0; r < zoneHeight; r++) {
//
//		newR1 = r + start1H;
//		newR2 = r + start2H;
//		for (int c = 0; c < zoneWidth; c++) {
//
//			newC1 = c + start1W;
//			newC2 = c + start2W;
//			total1 += unsigned int(img1[newR1 * width + newC1]);
//			total2 += unsigned int(img2[newR2 * width + newC2]);
//		}
//	}
//	mean1 = double(total1 / (zoneHeight * zoneWidth));
//	mean2 = double(total2 / (zoneHeight * zoneWidth));
//
//	for (int r = 0; r < zoneHeight; r++) {
//
//		newR1 = r + start1H;
//		newR2 = r + start2H;
//		for (int c = 0; c < zoneWidth; c++) {
//
//			newC1 = c + start1W;
//			newC2 = c + start2W;
//			total += ((img1[newR1 * width + newC1] - mean1) * (img2[newR2 * width + newC2] - mean2));
//
//			totalSqr1 += pow((img1[newR1 * width + newC1] - mean1), 2);
//			totalSqr2 += pow((img2[newR2 * width + newC2] - mean2), 2);
//
//			img1[newR1 * width + newC1] = v;
//			img2[newR2 * width + newC2] = v;
//		}
//	}
//
//	return float(total / sqrt(totalSqr1 + totalSqr2));
///}

/*
* cornerID
*
	0 ********* 1
	* 			*
	* 			*
	* 			*
	2 ********* 3
*/
int ZoneDetection(BYTE* img1, BYTE* img2, int width, int height, xy* vec, xy* pocDot, xy* signedShift, float* rotationDeg, float* fallbackScore, float* fallbackObjective, bool* fallbackApplied, double* fallbackLocalMs, double* fallbackGlobalMs, double* fallbackEdgeMs) {
	// Use wrapped phase-correlation displacement directly instead of corner-zone heuristic.
	(void)img1;
	(void)img2;

    if (fallbackScore != nullptr)
		*fallbackScore = -1.0f;
	if (fallbackObjective != nullptr)
		*fallbackObjective = -1.0f;
	if (fallbackApplied != nullptr)
		*fallbackApplied = false;
	if (fallbackLocalMs != nullptr) *fallbackLocalMs = -1.0;
	if (fallbackGlobalMs != nullptr) *fallbackGlobalMs = -1.0;
	if (fallbackEdgeMs != nullptr) *fallbackEdgeMs = -1.0;

	int dx = pocDot->x;
	int dy = pocDot->y;
	float rot = 0.0f;
	int anchorDx = dx;
	int anchorDy = dy;
	float anchorRot = rot;
	bool hasTranslationAnchor = false;

	if (dx > width / 2)
		dx -= width;
	if (dy > height / 2)
		dy -= height;

	float pocScore = EvaluateShiftNccSampled(img1, img2, width, height, dx, dy, 2);

	float currentScore = (rot == 0.0f)
		? EvaluateShiftNccSampled(img1, img2, width, height, dx, dy, 2)
		: EvaluateShiftNccSampledRotated(img1, img2, width, height, dx, dy, rot, 2);

	// Try edge-anchored search first on low-confidence cases to avoid getting trapped by high-overlap local maxima.
	bool edgeAccepted = false;
	if (pocScore < 0.70f) {
		float edgeScore = currentScore;
		float edgeObjective = currentScore + ShiftSelectionBias(width, height, dx, dy) + EdgeOverlapTargetBias(width, height, dx, dy);
		auto tedge0 = std::chrono::high_resolution_clock::now();
		FallbackEdgeAnchoredSearch(img1, img2, width, height, dx, dy, rot, currentScore, &edgeScore, &edgeObjective, &edgeAccepted);
		auto tedge1 = std::chrono::high_resolution_clock::now();
		if (fallbackEdgeMs != nullptr) {
			*fallbackEdgeMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(tedge1 - tedge0).count();
		}
		if (fallbackScore != nullptr)
			*fallbackScore = edgeScore;
		if (fallbackObjective != nullptr)
			*fallbackObjective = edgeObjective;
		if (fallbackApplied != nullptr)
			*fallbackApplied = edgeAccepted;

		if (edgeAccepted) {
			currentScore = (rot == 0.0f)
				? EvaluateShiftNccSampled(img1, img2, width, height, dx, dy, 2)
				: EvaluateShiftNccSampledRotated(img1, img2, width, height, dx, dy, rot, 2);
		}
	}

	if (pocScore < 0.68f) {
		int vertDx = dx;
		int vertDy = dy;
		float vertRot = rot;
		float vertScore = currentScore;
		float vertObjective = currentScore + ShiftSelectionBias(width, height, dx, dy) + EdgeOverlapTargetBias(width, height, dx, dy);
		bool vertAccepted = false;
		auto tvert0 = std::chrono::high_resolution_clock::now();
		FallbackVerticalAnchoredSearch(img1, img2, width, height, vertDx, vertDy, vertRot, currentScore, &vertScore, &vertObjective, &vertAccepted);
		auto tvert1 = std::chrono::high_resolution_clock::now();

		int horizDx = dx;
		int horizDy = dy;
		float horizRot = rot;
		float horizScore = currentScore;
		float horizObjective = currentScore + ShiftSelectionBias(width, height, dx, dy) + EdgeOverlapTargetBias(width, height, dx, dy);
		bool horizAccepted = false;
		auto thoriz0 = std::chrono::high_resolution_clock::now();
		FallbackHorizontalAnchoredSearch(img1, img2, width, height, horizDx, horizDy, horizRot, currentScore, &horizScore, &horizObjective, &horizAccepted);
		auto thoriz1 = std::chrono::high_resolution_clock::now();

		if (fallbackEdgeMs != nullptr && *fallbackEdgeMs < 0.0) {
			*fallbackEdgeMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(tvert1 - tvert0).count() +
				std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(thoriz1 - thoriz0).count();
		}

		bool chooseVertical = false;
		if (vertAccepted && horizAccepted) {
			chooseVertical = (vertObjective > horizObjective) || (vertObjective == horizObjective && vertScore >= horizScore);
		}
		else if (vertAccepted) {
			chooseVertical = true;
		}
		else if (!horizAccepted) {
			chooseVertical = true;
		}

		if (chooseVertical && vertAccepted) {
			dx = vertDx;
			dy = vertDy;
			rot = vertRot;
			anchorDx = dx;
			anchorDy = dy;
			anchorRot = rot;
			hasTranslationAnchor = true;
			currentScore = (rot == 0.0f)
				? EvaluateShiftNccSampled(img1, img2, width, height, dx, dy, 2)
				: EvaluateShiftNccSampledRotated(img1, img2, width, height, dx, dy, rot, 2);
			if (fallbackScore != nullptr)
				*fallbackScore = vertScore;
			if (fallbackObjective != nullptr)
				*fallbackObjective = vertObjective;
			if (fallbackApplied != nullptr)
				*fallbackApplied = true;
		}
		else if (horizAccepted) {
			dx = horizDx;
			dy = horizDy;
			rot = horizRot;
			anchorDx = dx;
			anchorDy = dy;
			anchorRot = rot;
			hasTranslationAnchor = true;
			currentScore = (rot == 0.0f)
				? EvaluateShiftNccSampled(img1, img2, width, height, dx, dy, 2)
				: EvaluateShiftNccSampledRotated(img1, img2, width, height, dx, dy, rot, 2);
			if (fallbackScore != nullptr)
				*fallbackScore = horizScore;
			if (fallbackObjective != nullptr)
				*fallbackObjective = horizObjective;
			if (fallbackApplied != nullptr)
				*fallbackApplied = true;
		}
	}

	if (pocScore < 0.70f && !edgeAccepted) {
		auto tloc0 = std::chrono::high_resolution_clock::now();
		FallbackLocalNccSearch(img1, img2, width, height, dx, dy, pocScore);
		auto tloc1 = std::chrono::high_resolution_clock::now();
		if (fallbackLocalMs != nullptr) {
			*fallbackLocalMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(tloc1 - tloc0).count();
		}

		float localScore = EvaluateShiftNccSampled(img1, img2, width, height, dx, dy, 2);
		if (localScore < 0.55f) {
			auto tglob0 = std::chrono::high_resolution_clock::now();
			FallbackGlobalNccSearch(img1, img2, width, height, dx, dy, rot, localScore);
			auto tglob1 = std::chrono::high_resolution_clock::now();
			if (fallbackGlobalMs != nullptr) {
				*fallbackGlobalMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(tglob1 - tglob0).count();
			}
		}
	}

	currentScore = (rot == 0.0f)
		? EvaluateShiftNccSampled(img1, img2, width, height, dx, dy, 2)
		: EvaluateShiftNccSampledRotated(img1, img2, width, height, dx, dy, rot, 2);
	bool shiftTooSmall = (abs(dx) < (width / 14)) && (abs(dy) < (height / 14));
	if ((currentScore < 0.62f || shiftTooSmall) && !edgeAccepted) {
		float edgeScore = currentScore;
		float edgeObjective = currentScore + ShiftSelectionBias(width, height, dx, dy) + EdgeOverlapTargetBias(width, height, dx, dy);
		bool edgeAcceptedRetry = false;
		auto tedge0 = std::chrono::high_resolution_clock::now();
		FallbackEdgeAnchoredSearch(img1, img2, width, height, dx, dy, rot, currentScore, &edgeScore, &edgeObjective, &edgeAcceptedRetry);
		auto tedge1 = std::chrono::high_resolution_clock::now();
		if (fallbackEdgeMs != nullptr) {
			*fallbackEdgeMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(tedge1 - tedge0).count();
		}
		if (fallbackScore != nullptr)
			*fallbackScore = edgeScore;
		if (fallbackObjective != nullptr)
			*fallbackObjective = edgeObjective;
		if (fallbackApplied != nullptr)
			*fallbackApplied = edgeAcceptedRetry;
	}

	if (currentScore < 0.80f || (fallbackApplied != nullptr && *fallbackApplied)) {
		auto tref0 = std::chrono::high_resolution_clock::now();
		RefineShiftRotationLocal(img1, img2, width, height, dx, dy, rot);
		auto tref1 = std::chrono::high_resolution_clock::now();
		currentScore = (rot == 0.0f)
			? EvaluateShiftNccSampled(img1, img2, width, height, dx, dy, 2)
			: EvaluateShiftNccSampledRotated(img1, img2, width, height, dx, dy, rot, 2);
		if (fallbackEdgeMs != nullptr) {
			double refineMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(tref1 - tref0).count();
			if (*fallbackEdgeMs < 0.0) *fallbackEdgeMs = refineMs;
			else *fallbackEdgeMs += refineMs;
		}
	}

	if (currentScore < 0.78f || (abs(dx) < width / 18 || abs(dy) < height / 18)) {
		auto ttrans0 = std::chrono::high_resolution_clock::now();
		RefineAxisDominantTranslation(img1, img2, width, height, dx, dy, rot);
		RefineCrossAxisForVerticalSeam(img1, img2, width, height, dx, dy);
		auto ttrans1 = std::chrono::high_resolution_clock::now();
		currentScore = EvaluateShiftNccSampled(img1, img2, width, height, dx, dy, 2);
		if (fallbackEdgeMs != nullptr) {
			double trMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(ttrans1 - ttrans0).count();
			if (*fallbackEdgeMs < 0.0) *fallbackEdgeMs = trMs;
			else *fallbackEdgeMs += trMs;
		}
	}

	// If later refinements collapse cross-axis motion in a vertical stitch, keep the stronger fallback anchor.
	if (hasTranslationAnchor) {
		const bool verticalDominantAnchor = abs(anchorDy) >= abs(anchorDx) * 2;
		if (verticalDominantAnchor && abs(anchorDy) > height / 5) {
			float anchorEdge = EvaluateVerticalSeamEdgeScore(img1, img2, width, height, anchorDx, anchorDy, 3);
			float currentEdge = EvaluateVerticalSeamEdgeScore(img1, img2, width, height, dx, dy, 3);
			float anchorNcc = EvaluateShiftNccSampled(img1, img2, width, height, anchorDx, anchorDy, 2);
			float currentNcc = EvaluateShiftNccSampled(img1, img2, width, height, dx, dy, 2);

			float anchorObj = 0.60f * anchorEdge + 0.40f * anchorNcc;
			float currentObj = 0.60f * currentEdge + 0.40f * currentNcc;

			if (abs(dx) + 3 < abs(anchorDx) && anchorObj >= (currentObj - 0.010f)) {
				dx = anchorDx;
				dy = anchorDy;
				rot = anchorRot;
			}
		}
	}

	// Translation fallback should not carry tiny rotational noise in dominant-vertical cases.
	if (abs(dy) >= abs(dx) * 2 && fabs(rot) <= 2.2f)
		rot = 0.0f;

	// Final guard: in dominant-vertical low-confidence scenarios, run a dense dx ensemble to avoid dx collapsing to zero.
	if (pocScore < 0.20f && abs(dy) > height / 3)
		EnforceVerticalSeamDxEnsemble(img1, img2, width, height, dx, dy);

	if (pocScore < 0.20f && abs(dy) > height / 3)
		ResolveVerticalDxSignLowConfidence(img1, img2, width, height, dx, dy);

	if (signedShift != nullptr) {
		signedShift->x = dx;
		signedShift->y = dy;
	}
	if (rotationDeg != nullptr) {
		*rotationDeg = rot;
	}

	vec->x = (dx < 0) ? -dx : dx;
	vec->y = (dy < 0) ? -dy : dy;

	int corner = 0;
	if (dx >= 0 && dy >= 0)
		corner = 0;
	else if (dx < 0 && dy >= 0)
		corner = 1;
	else if (dx >= 0 && dy < 0)
		corner = 2;
	else
		corner = 3;

	// Keep pocDot synchronized with the overlap anchor expected by Rand4Dots.
	// This prevents selecting feature points from non-overlap areas when using wrapped dx/dy.
	switch (corner)
	{
	case 0:
		pocDot->x = vec->x;
		pocDot->y = vec->y;
		break;
	case 1:
		pocDot->x = width - vec->x;
		pocDot->y = vec->y;
		break;
	case 2:
		pocDot->x = vec->x;
		pocDot->y = height - vec->y;
		break;
	case 3:
		pocDot->x = width - vec->x;
		pocDot->y = height - vec->y;
		break;
	default:
		break;
	}

	if (pocDot->x < 0) pocDot->x = 0;
	if (pocDot->x > width - 1) pocDot->x = width - 1;
	if (pocDot->y < 0) pocDot->y = 0;
	if (pocDot->y > height - 1) pocDot->y = height - 1;

	return corner;
}


