#include "Correlation.h"
#include "Process.h"
#include <math.h>
#include <vector>
#include <cstdint>


double* Conjugate(double* imaginary, int width, int height) {

	long imageSize = width * height;
	double* conjugated = new double[imageSize];

#pragma omp parallel num_threads(NUM_THREADS) shared(imageSize, conjugated, imaginary)
	{
#pragma omp for schedule(dynamic) nowait
		for (long i = 0; i < imageSize; i++)
			conjugated[i] = -imaginary[i];
	}

	return conjugated;
}

void ComplexMult(double* fft1Real, double* fft1Imag, double* fft2Real, double* fft2Imag, double* outReal, double* outImag, int width, int height) {

	long imageSize = width * height;

#pragma omp parallel num_threads(NUM_THREADS) shared(imageSize, fft1Real, fft2Real, fft1Imag, fft2Imag, outReal, outImag)
	{
#pragma omp for schedule(dynamic) nowait
		for (long i = 0; i < imageSize; i++)
		{
			outReal[i] = fft1Real[i] * fft2Real[i] - fft1Imag[i] * fft2Imag[i];
			outImag[i] = fft1Real[i] * fft2Imag[i] + fft1Imag[i] * fft2Real[i];
		}
	}
}

double* PhaseCorrelation(double* fft1Real, double* fft1Imag, double* fft2Real, double* fft2Imag, int width, int height)
{
	double* multReal, * multImag;
	long imageSize = width * height;

	multReal = new double[imageSize];
	multImag = new double[imageSize];

	double* conjugated = Conjugate(fft2Imag, width, height);
	ComplexMult(fft1Real, fft1Imag, fft2Real, conjugated, multReal, multImag, width, height);

	double norm;
#pragma omp parallel num_threads(NUM_THREADS) shared(multReal, multImag, imageSize) private(norm)
	{
#pragma omp for schedule(dynamic) nowait
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

#pragma omp parallel num_threads(NUM_THREADS) shared(zoneHeight,zoneWidth,start1H,start2H,start1W,start2W, width, img1, img2) private(newR1, newR2, newC1, newC2)
	{
#pragma omp for schedule(dynamic) reduction(+:total1,total2) nowait
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

#pragma omp parallel num_threads(NUM_THREADS) shared(zoneHeight,zoneWidth,start1H,start2H,start1W,start2W,width,img1,img2,mean1,mean2) private(newR1, newR2, newC1, newC2)
	{
#pragma omp for schedule(dynamic) reduction(+:total,totalSqr1,totalSqr2) nowait
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

	const float targetOverlapPx = 300.0f;
	float diff = (float)fabs((float)primaryOverlap - targetOverlapPx);
	float bias = -0.28f * (diff / targetOverlapPx);

	if (primaryOverlap > (int)(targetOverlapPx * 2.2f))
		bias -= 0.30f;
	if (primaryOverlap < 90)
		bias -= 0.20f;

	int dominantShift = horizontalNeighbor ? absDx : absDy;
	int dominantDim = horizontalNeighbor ? width : height;
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

#pragma omp parallel num_threads(NUM_THREADS)
	{
		int tBestDx2 = bestDx2;
		int tBestDy2 = bestDy2;
		float tBest2 = best2;
		float tBestObjective2 = bestObjective2;
		float tBestAngle2 = bestAngle2;

#pragma omp for collapse(3) schedule(dynamic) nowait
		for (int ai = 0; ai < 5; ai++) {
			for (int cy2 = -h2 / 2; cy2 <= h2 / 2; cy2 += coarseStep2) {
				for (int cx2 = -w2 / 2; cx2 <= w2 / 2; cx2 += coarseStep2) {
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

#pragma omp parallel num_threads(NUM_THREADS)
	{
		int tBestDx = bestDx;
		int tBestDy = bestDy;
		float tBest = best;
		float tBestObjective = bestObjective;

#pragma omp for collapse(2) schedule(dynamic) nowait
		for (int cy = candidateDy - 16; cy <= candidateDy + 16; cy++) {
			for (int cx = candidateDx - 16; cx <= candidateDx + 16; cx++) {
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

#pragma omp parallel num_threads(NUM_THREADS)
	{
		float tBest = best;
		float tBestObjective = bestObjective;
		int tBestDx = bestDx;
		int tBestDy = bestDy;

#pragma omp for collapse(2) schedule(dynamic) nowait
		for (int cy = minDy; cy <= maxDy; cy += coarseStep) {
			for (int cx = minDx; cx <= maxDx; cx += coarseStep) {
				float s = EvaluateShiftNccSampled(img1, img2, width, height, cx, cy, 4);
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

#pragma omp parallel num_threads(NUM_THREADS)
	{
		float tBest = best;
		float tBestObjective = bestObjective;
		int tBestDx = bestDx;
		int tBestDy = bestDy;

#pragma omp for collapse(2) schedule(dynamic) nowait
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

	const int overlapPxCandidates[] = { 140, 180, 220, 260, 300, 340, 380, 460, 560 };
	const float overlapRatioCandidates[] = { 0.05f, 0.07f, 0.09f, 0.12f, 0.16f, 0.20f, 0.24f };
	const float angleCandidates[] = { -6.0f, -4.0f, -2.0f, 0.0f, 2.0f, 4.0f, 6.0f };

	int bestDx = dx;
	int bestDy = dy;
	float bestAng = angleDeg;
	float bestScore = baseScore;
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

#pragma omp parallel num_threads(NUM_THREADS)
	{
		int tBestDx = bestDx;
		int tBestDy = bestDy;
		float tBestAng = bestAng;
		float tBestScore = bestScore;
		float tBestObjective = bestObjective;

#pragma omp for collapse(2) schedule(dynamic) nowait
		for (int ci = 0; ci < (int)candidates.size(); ci++) {
			for (int ai = 0; ai < 7; ai++) {
				int candDx = candidates[(size_t)ci].x;
				int candDy = candidates[(size_t)ci].y;
				float candAng = angleCandidates[ai];
				float s = (candAng == 0.0f)
					? EvaluateShiftNccSampled(img1, img2, width, height, candDx, candDy, 2)
					: EvaluateShiftNccSampledRotated(img1, img2, width, height, candDx, candDy, candAng, 2);
				float objective = s + ShiftSelectionBias(width, height, candDx, candDy) + EdgeOverlapTargetBias(width, height, candDx, candDy) - (float)fabs(candAng) * 0.005f + 0.06f;
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

	const bool veryLowConfidence = baseScore < 0.35f;
	bool accept = false;
	if (veryLowConfidence) {
		accept = (bestObjective > baseObjective + 0.005f) && (bestScore > baseScore - 0.02f);
	}
	else {
		accept = (bestObjective > baseObjective + 0.02f) && (bestScore > 0.26f);
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
int ZoneDetection(BYTE* img1, BYTE* img2, int width, int height, xy* vec, xy* pocDot, xy* signedShift, float* rotationDeg, float* fallbackScore, float* fallbackObjective, bool* fallbackApplied) {
	// Use wrapped phase-correlation displacement directly instead of corner-zone heuristic.
	(void)img1;
	(void)img2;

	if (fallbackScore != nullptr)
		*fallbackScore = -1.0f;
	if (fallbackObjective != nullptr)
		*fallbackObjective = -1.0f;
	if (fallbackApplied != nullptr)
		*fallbackApplied = false;

	int dx = pocDot->x;
	int dy = pocDot->y;
	float rot = 0.0f;

	if (dx > width / 2)
		dx -= width;
	if (dy > height / 2)
		dy -= height;

	float pocScore = EvaluateShiftNccSampled(img1, img2, width, height, dx, dy, 2);
	if (pocScore < 0.70f) {
		FallbackLocalNccSearch(img1, img2, width, height, dx, dy, pocScore);
		float localScore = EvaluateShiftNccSampled(img1, img2, width, height, dx, dy, 2);
		if (localScore < 0.55f) {
			FallbackGlobalNccSearch(img1, img2, width, height, dx, dy, rot, localScore);
		}
	}

	float currentScore = (rot == 0.0f)
		? EvaluateShiftNccSampled(img1, img2, width, height, dx, dy, 2)
		: EvaluateShiftNccSampledRotated(img1, img2, width, height, dx, dy, rot, 2);
	bool shiftTooSmall = (abs(dx) < (width / 14)) && (abs(dy) < (height / 14));
	if (currentScore < 0.62f || shiftTooSmall) {
		float edgeScore = currentScore;
		float edgeObjective = currentScore + ShiftSelectionBias(width, height, dx, dy) + EdgeOverlapTargetBias(width, height, dx, dy);
		bool edgeAccepted = false;
		FallbackEdgeAnchoredSearch(img1, img2, width, height, dx, dy, rot, currentScore, &edgeScore, &edgeObjective, &edgeAccepted);
		if (fallbackScore != nullptr)
			*fallbackScore = edgeScore;
		if (fallbackObjective != nullptr)
			*fallbackObjective = edgeObjective;
		if (fallbackApplied != nullptr)
			*fallbackApplied = edgeAccepted;
	}

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

