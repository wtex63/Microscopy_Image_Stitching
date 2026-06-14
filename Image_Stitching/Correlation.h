#pragma once
#include <omp.h>
#include "Definitions.h"

enum PhaseCorrelationBackend
{
	PhaseCorrelationBackendCpu = 0,
	PhaseCorrelationBackendGpu = 1
};

double* PhaseCorrelation(double* fft1Real, double* fft1Imag, double* fft2Real, double* fft2Imag, int width, int height);
void SetPhaseCorrelationBackend(PhaseCorrelationBackend backend);
PhaseCorrelationBackend GetPhaseCorrelationBackend();
bool IsPhaseCorrelationGpuAvailable();
const char* GetPhaseCorrelationBackendName();
float Correlation(BYTE* img1, BYTE* img2, int width, int height, int zoneWidth, int zoneHeight, int start1H, int start1W, int start2H, int start2W);
float PhaseShiftConfidence(BYTE* img1, BYTE* img2, int width, int height, xy* pocDot);
// ZoneDetection now returns additional timing diagnostics (milliseconds) for fallback stages.
// last three pointer params are optional and may be null.
int ZoneDetection(BYTE* img1, BYTE* img2, int width, int height, xy* vec, xy* pocDot, xy* signedShift, float* rotationDeg, float* fallbackScore, float* fallbackObjective, bool* fallbackApplied, double* fallbackLocalMs = nullptr, double* fallbackGlobalMs = nullptr, double* fallbackEdgeMs = nullptr);

// Toggle for hard-overlap constraint
void SetHardOverlapCheck(bool enabled);
bool GetHardOverlapCheck();
//float Correlation(BYTE* img1, BYTE* img2, int width, int height, int zoneWidth, int zoneHeight, int start1H, int start1W, int start2H, int start2W, BYTE v);