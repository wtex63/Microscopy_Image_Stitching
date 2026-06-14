#pragma once
#include <math.h>
#include <cstdlib> 
#include <ctime> 
#include <vector>
#include <algorithm>
#include <cstdint>
#include "Definitions.h"
#define MIN(a,b) ((a) < (b) ? (a) : (b))


/*
* cornerID
*
	0 ********* 1
	* 		    *
	* 		    *
	* 		    *
	2 ********* 3
*/
xy* Rand4Dots(size_t cornerID, xy* pocDot, int imgWidth, int imgHeight)
{
	int centerX, centerY, areaSizeX, areaSizeY, radius;

	switch (cornerID)
	{
	case 0:
		areaSizeX = (imgWidth - 1) - pocDot->x;
		areaSizeY = (imgHeight - 1) - pocDot->y;
		centerX = pocDot->x + (int)(areaSizeX / 2);
		centerY = pocDot->y + (int)(areaSizeY / 2);
		break;

	case 1:
		areaSizeX = pocDot->x;
		areaSizeY = (imgHeight - 1) - pocDot->y;
		centerX = (int)(areaSizeX / 2);
		centerY = pocDot->y + (int)(areaSizeY / 2);
		break;

	case 2:
		areaSizeX = (imgWidth - 1) - pocDot->x;
		areaSizeY = pocDot->y;
		centerX = pocDot->x + (int)(areaSizeX / 2);
		centerY = (int)(areaSizeY / 2);
		break;

	case 3:
		areaSizeX = pocDot->x;
		areaSizeY = pocDot->y;
		centerX = (int)(areaSizeX / 2);
		centerY = (int)(areaSizeY / 2);
		break;

	default:
		return NULL;
	}

	radius = MIN((int)(areaSizeX / 2), (int)(areaSizeY / 2));
	radius--;

	if (radius < 2)
		return NULL;

	xy* rDots = new xy[4]();
	rDots[0].x = centerX + radius;
	rDots[0].y = centerY;
	rDots[1].x = centerX;
	rDots[1].y = centerY - radius;
	rDots[2].x = centerX - radius;
	rDots[2].y = centerY + 1;
	rDots[3].x = centerX + 1;
	rDots[3].y = centerY + radius;

	return rDots;
}


/*
* cornerID
*
	0 ********* 1
	* 		    *
	* 		    *
	* 		    *
	2 ********* 3
*/
xy* MatchingDots(size_t cornerID, xy* img1Dots, xy* vec) {

	xy* rDots = new xy[4]();

	switch (cornerID)
	{
	case 0:
		rDots[0].x = img1Dots[0].x - vec->x;
		rDots[0].y = img1Dots[0].y - vec->y;
		rDots[1].x = img1Dots[1].x - vec->x;
		rDots[1].y = img1Dots[1].y - vec->y;
		rDots[2].x = img1Dots[2].x - vec->x;
		rDots[2].y = img1Dots[2].y - vec->y;
		rDots[3].x = img1Dots[3].x - vec->x;
		rDots[3].y = img1Dots[3].y - vec->y;
		break;

	case 1:
		rDots[0].x = img1Dots[0].x + vec->x;
		rDots[0].y = img1Dots[0].y - vec->y;
		rDots[1].x = img1Dots[1].x + vec->x;
		rDots[1].y = img1Dots[1].y - vec->y;
		rDots[2].x = img1Dots[2].x + vec->x;
		rDots[2].y = img1Dots[2].y - vec->y;
		rDots[3].x = img1Dots[3].x + vec->x;
		rDots[3].y = img1Dots[3].y - vec->y;
		break;

	case 2:
		rDots[0].x = img1Dots[0].x - vec->x;
		rDots[0].y = img1Dots[0].y + vec->y;
		rDots[1].x = img1Dots[1].x - vec->x;
		rDots[1].y = img1Dots[1].y + vec->y;
		rDots[2].x = img1Dots[2].x - vec->x;
		rDots[2].y = img1Dots[2].y + vec->y;
		rDots[3].x = img1Dots[3].x - vec->x;
		rDots[3].y = img1Dots[3].y + vec->y;
		break;

	case 3:
		rDots[0].x = img1Dots[0].x + vec->x;
		rDots[0].y = img1Dots[0].y + vec->y;
		rDots[1].x = img1Dots[1].x + vec->x;
		rDots[1].y = img1Dots[1].y + vec->y;
		rDots[2].x = img1Dots[2].x + vec->x;
		rDots[2].y = img1Dots[2].y + vec->y;
		rDots[3].x = img1Dots[3].x + vec->x;
		rDots[3].y = img1Dots[3].y + vec->y;
		break;

	default:
		break;
	}

	return rDots;
}


/*
* cornerID
*
	0 ********* 1
	* 		    *
	* 		    *
	* 		    *
	2 ********* 3
*/
xy* PanoDots(xy* prevVec, size_t currCornerID, xy* img1Dots) {

	xy* rDots = new xy[4]();

	rDots[0].x = img1Dots[0].x + prevVec->x;
	rDots[0].y = img1Dots[0].y + prevVec->y;
	rDots[1].x = img1Dots[1].x + prevVec->x;
	rDots[1].y = img1Dots[1].y + prevVec->y;
	rDots[2].x = img1Dots[2].x + prevVec->x;
	rDots[2].y = img1Dots[2].y + prevVec->y;
	rDots[3].x = img1Dots[3].x + prevVec->x;
	rDots[3].y = img1Dots[3].y + prevVec->y;

	return rDots;
}

static inline int PopCount64(uint64_t v)
{
	int c = 0;
	while (v) {
		v &= (v - 1);
		c++;
	}
	return c;
}

static inline uint64_t BuildBinaryDescriptor64(BYTE* img, int width, int height, int x, int y)
{
	static const int pairs[64][4] = {
		{-4,-1, 4, 1}, {-3,-3, 3, 3}, {-4, 0, 4, 0}, {0,-4, 0, 4},
		{-2,-4, 2, 4}, {-4,-2, 4, 2}, {-1,-3, 1, 3}, {-3,-1, 3, 1},
		{-4, 1, 4,-1}, {-3, 3, 3,-3}, {-2, 0, 2, 0}, {0,-2, 0, 2},
		{-2,-2, 2, 2}, {-2, 2, 2,-2}, {-4,-4, 4, 4}, {-4, 4, 4,-4},
		{-1,-4, 1, 4}, {-4,-1, 4, 1}, {-3,-2, 3, 2}, {-2,-3, 2, 3},
		{-1,-1, 1, 1}, {-1, 1, 1,-1}, {-3, 0, 3, 0}, {0,-3, 0, 3},
		{-4, 2, 4,-2}, {-2,-4, 2, 4}, {-3, 1, 3,-1}, {-1,-3, 1, 3},
		{-2, 1, 2,-1}, {-1,-2, 1, 2}, {-4,-3, 4, 3}, {-3,-4, 3, 4},
		{-4, 0, 0, 4}, {0,-4, 4, 0}, {-3, 0, 0, 3}, {0,-3, 3, 0},
		{-2, 0, 0, 2}, {0,-2, 2, 0}, {-1, 0, 0, 1}, {0,-1, 1, 0},
		{-4, 1, 0, 4}, {-1,-4, 4, 1}, {-3, 2, 2, 3}, {-2,-3, 3, 2},
		{-4,-1, 0,-4}, {1, 4, 4, 1}, {-3,-2, 0,-3}, {2, 3, 3, 2},
		{-2,-1, 1, 2}, {-1,-2, 2, 1}, {-4, 3, 3, 4}, {-3, 4, 4, 3},
		{-4,-2,-2,-4}, {2, 4, 4, 2}, {-3,-1,-1,-3}, {1, 3, 3, 1},
		{-2, 2, 1, 4}, {-1, 4, 2, 2}, {-4, 1,-1, 4}, {1,-4, 4,-1}
	};

	uint64_t d = 0;
	for (int i = 0; i < 64; i++) {
		int x1 = x + pairs[i][0];
		int y1 = y + pairs[i][1];
		int x2 = x + pairs[i][2];
		int y2 = y + pairs[i][3];
		BYTE a = img[y1 * width + x1];
		BYTE b = img[y2 * width + x2];
		if (a < b)
			d |= (uint64_t(1) << i);
	}
	return d;
}

static inline bool FindFeatureMatchesRansac(BYTE* img1, BYTE* img2, int width, int height, xy* outImg1Dots, xy* outImg2Dots, float& inlierRatio, xy* outShift)
{
	if (img1 == nullptr || img2 == nullptr || outImg1Dots == nullptr || outImg2Dots == nullptr || width < 40 || height < 40)
		return false;

	struct Candidate { int x; int y; int score; };
	std::vector<Candidate> c1;
	std::vector<Candidate> c2;
	c1.reserve(4000);
	c2.reserve(4000);

	for (int y = 5; y < height - 5; y += 4) {
		for (int x = 5; x < width - 5; x += 4) {
			int idx = y * width + x;
			int gx1 = abs((int)img1[idx + 1] - (int)img1[idx - 1]);
			int gy1 = abs((int)img1[idx + width] - (int)img1[idx - width]);
			int gd11 = abs((int)img1[idx + width + 1] - (int)img1[idx - width - 1]);
			int gd12 = abs((int)img1[idx + width - 1] - (int)img1[idx - width + 1]);
			int s1 = gx1 + gy1 + ((gd11 + gd12) / 2);
			if (s1 > 60) c1.push_back({ x, y, s1 });

			int gx2 = abs((int)img2[idx + 1] - (int)img2[idx - 1]);
			int gy2 = abs((int)img2[idx + width] - (int)img2[idx - width]);
			int gd21 = abs((int)img2[idx + width + 1] - (int)img2[idx - width - 1]);
			int gd22 = abs((int)img2[idx + width - 1] - (int)img2[idx - width + 1]);
			int s2 = gx2 + gy2 + ((gd21 + gd22) / 2);
			if (s2 > 60) c2.push_back({ x, y, s2 });
		}
	}

	auto EvalNccShift = [](BYTE* a, BYTE* b, int w, int h, int dx, int dy, int step) -> float {
		int xStart = dx > 0 ? dx : 0;
		int yStart = dy > 0 ? dy : 0;
		int xEnd = (w + dx - 1 < w - 1) ? (w + dx - 1) : (w - 1);
		int yEnd = (h + dy - 1 < h - 1) ? (h + dy - 1) : (h - 1);

		if (xStart >= xEnd || yStart >= yEnd)
			return -2.0f;

		double s1 = 0.0, s2 = 0.0, s11 = 0.0, s22 = 0.0, s12 = 0.0;
		int n = 0;
		for (int y = yStart; y <= yEnd; y += step) {
			int y2 = y - dy;
			for (int x = xStart; x <= xEnd; x += step) {
				int x2 = x - dx;
				double v1 = (double)a[y * w + x];
				double v2 = (double)b[y2 * w + x2];
				s1 += v1;
				s2 += v2;
				s11 += v1 * v1;
				s22 += v2 * v2;
				s12 += v1 * v2;
				n++;
			}
		}

		if (n < 64)
			return -2.0f;

		double nf = (double)n;
		double num = s12 - (s1 * s2) / nf;
		double den1 = s11 - (s1 * s1) / nf;
		double den2 = s22 - (s2 * s2) / nf;
		double den = sqrt(den1 * den2);
		if (den <= 1e-12)
			return -2.0f;
		return (float)(num / den);
	};

	auto BuildDotsFromShift = [&](int dx, int dy, float score) -> bool {
		int xStart = dx > 0 ? dx : 0;
		int yStart = dy > 0 ? dy : 0;
		int xEnd = (width + dx - 1 < width - 1) ? (width + dx - 1) : (width - 1);
		int yEnd = (height + dy - 1 < height - 1) ? (height + dy - 1) : (height - 1);

		if (xStart >= xEnd || yStart >= yEnd)
			return false;

		int margin = 6;
		xStart += margin; yStart += margin;
		xEnd -= margin; yEnd -= margin;
		if (xEnd - xStart < 10 || yEnd - yStart < 10)
			return false;

		outImg1Dots[0] = { xStart, yStart };
		outImg1Dots[1] = { xEnd, yStart };
		outImg1Dots[2] = { xStart, yEnd };
		outImg1Dots[3] = { xEnd, yEnd };

		for (int i = 0; i < 4; i++) {
			outImg2Dots[i].x = outImg1Dots[i].x - dx;
			outImg2Dots[i].y = outImg1Dots[i].y - dy;
			if (outImg2Dots[i].x < 0 || outImg2Dots[i].x >= width || outImg2Dots[i].y < 0 || outImg2Dots[i].y >= height)
				return false;
		}

		if (outShift != nullptr) {
			outShift->x = abs(dx);
			outShift->y = abs(dy);
		}

		inlierRatio = score;
		if (inlierRatio < 0.0f) inlierRatio = 0.0f;
		if (inlierRatio > 1.0f) inlierRatio = 1.0f;
		return true;
	};

	auto TryNccFallback = [&]() -> bool {
		int ds = (width > height ? width : height) / 320;
		if (ds < 1) ds = 1;

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
			int oy = y * ds;
			for (int x = 0; x < w2; x++) {
				int ox = x * ds;
				a2[(size_t)y * (size_t)w2 + (size_t)x] = img1[oy * width + ox];
				b2[(size_t)y * (size_t)w2 + (size_t)x] = img2[oy * width + ox];
			}
		}

		int bestDx2 = 0, bestDy2 = 0;
		float best2 = -2.0f;
		int step2 = 2;
		for (int dy2 = -h2 / 2; dy2 <= h2 / 2; dy2 += step2) {
			for (int dx2 = -w2 / 2; dx2 <= w2 / 2; dx2 += step2) {
				float s = EvalNccShift(a2.data(), b2.data(), w2, h2, dx2, dy2, 2);
				if (s > best2) {
					best2 = s;
					bestDx2 = dx2;
					bestDy2 = dy2;
				}
			}
		}

		int bestDx = bestDx2 * ds;
		int bestDy = bestDy2 * ds;
		float best = -2.0f;
		for (int dy = bestDy - 12; dy <= bestDy + 12; dy++) {
			for (int dx = bestDx - 12; dx <= bestDx + 12; dx++) {
				float s = EvalNccShift(img1, img2, width, height, dx, dy, 8);
				if (s > best) {
					best = s;
					bestDx = dx;
					bestDy = dy;
				}
			}
		}

		if (best < 0.40f)
			return false;

		return BuildDotsFromShift(bestDx, bestDy, best);
	};

	if (c1.size() < 20 || c2.size() < 20)
		return TryNccFallback();

	std::sort(c1.begin(), c1.end(), [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
	std::sort(c2.begin(), c2.end(), [](const Candidate& a, const Candidate& b) { return a.score > b.score; });

	const int maxFeat = 220;
	if ((int)c1.size() > maxFeat) c1.resize(maxFeat);
	if ((int)c2.size() > maxFeat) c2.resize(maxFeat);

	struct Feature { int x; int y; uint64_t d; };
	std::vector<Feature> f1;
	std::vector<Feature> f2;
	f1.reserve(c1.size());
	f2.reserve(c2.size());

	for (size_t i = 0; i < c1.size(); i++)
		f1.push_back({ c1[i].x, c1[i].y, BuildBinaryDescriptor64(img1, width, height, c1[i].x, c1[i].y) });
	for (size_t i = 0; i < c2.size(); i++)
		f2.push_back({ c2[i].x, c2[i].y, BuildBinaryDescriptor64(img2, width, height, c2[i].x, c2[i].y) });

	struct Match { int i1; int i2; int dist; };
	std::vector<Match> matches;
	matches.reserve(f1.size());

	for (size_t i = 0; i < f1.size(); i++) {
		int best = 9999, second = 9999, bestJ = -1;
		for (size_t j = 0; j < f2.size(); j++) {
			int d = PopCount64(f1[i].d ^ f2[j].d);
			if (d < best) {
				second = best;
				best = d;
				bestJ = (int)j;
			}
			else if (d < second) {
				second = d;
			}
		}

		if (bestJ >= 0 && best <= 24 && (float)best < 0.82f * (float)(second > 0 ? second : 1)) {
			matches.push_back({ (int)i, bestJ, best });
		}
	}

	if (matches.size() < 8)
		return TryNccFallback();

	static bool s_seeded = false;
	if (!s_seeded) {
		srand((unsigned int)time(NULL));
		s_seeded = true;
	}
	int bestDx = 0, bestDy = 0, bestInliers = 0;
	std::vector<int> inlierIdx;

	for (int it = 0; it < 120; it++) {
		int m = rand() % (int)matches.size();
		int dx = f1[matches[m].i1].x - f2[matches[m].i2].x;
		int dy = f1[matches[m].i1].y - f2[matches[m].i2].y;

		int inliers = 0;
		for (size_t k = 0; k < matches.size(); k++) {
			int px = f1[matches[k].i1].x - f2[matches[k].i2].x;
			int py = f1[matches[k].i1].y - f2[matches[k].i2].y;
			if (abs(px - dx) <= 4 && abs(py - dy) <= 4)
				inliers++;
		}

		if (inliers > bestInliers) {
			bestInliers = inliers;
			bestDx = dx;
			bestDy = dy;
		}
	}

	if (bestInliers < 6)
		return TryNccFallback();

	inlierIdx.clear();
	for (size_t k = 0; k < matches.size(); k++) {
		int px = f1[matches[k].i1].x - f2[matches[k].i2].x;
		int py = f1[matches[k].i1].y - f2[matches[k].i2].y;
		if (abs(px - bestDx) <= 4 && abs(py - bestDy) <= 4)
			inlierIdx.push_back((int)k);
	}

	inlierRatio = (float)inlierIdx.size() / (float)matches.size();
	if (inlierRatio < 0.35f || inlierIdx.size() < 6)
		return TryNccFallback();

	int minX = width - 1, minY = height - 1, maxX = 0, maxY = 0;
	for (size_t t = 0; t < inlierIdx.size(); t++) {
		int k = inlierIdx[t];
		int x = f1[matches[k].i1].x;
		int y = f1[matches[k].i1].y;
		if (x < minX) minX = x;
		if (x > maxX) maxX = x;
		if (y < minY) minY = y;
		if (y > maxY) maxY = y;
	}

	if (maxX - minX < 12 || maxY - minY < 12)
		return TryNccFallback();

	outImg1Dots[0] = { minX, minY };
	outImg1Dots[1] = { maxX, minY };
	outImg1Dots[2] = { minX, maxY };
	outImg1Dots[3] = { maxX, maxY };

	for (int i = 0; i < 4; i++) {
		outImg2Dots[i].x = outImg1Dots[i].x - bestDx;
		outImg2Dots[i].y = outImg1Dots[i].y - bestDy;
		if (outImg2Dots[i].x < 0 || outImg2Dots[i].x >= width || outImg2Dots[i].y < 0 || outImg2Dots[i].y >= height)
			return TryNccFallback();
	}

	if (outShift != nullptr) {
		outShift->x = abs(bestDx);
		outShift->y = abs(bestDy);
	}

	return true;
}


/*
* cornerID
*
	0 ********* 1
	* 		    *
	* 		    *
	* 		    *
	2 ********* 3
*/
void UpdatePrevVec(int currCornerID, xy* prevVec, xy* currVec) {

	switch (currCornerID)
	{
	case 0:
		prevVec->x += currVec->x;
		prevVec->y += currVec->y;
		break;

	case 1:
		prevVec->x -= currVec->x;
		prevVec->y += currVec->y;
		break;

	case 2:
		prevVec->x += currVec->x;
		prevVec->y -= currVec->y;
		break;

	case 3:
		prevVec->x -= currVec->x;
		prevVec->y -= currVec->y;
		break;

	default:
		break;
	}

}