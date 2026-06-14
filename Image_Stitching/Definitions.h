#pragma once

#include <omp.h>

// Use all available OpenMP worker threads instead of a fixed small count.
#define NUM_THREADS omp_get_max_threads()
typedef unsigned char BYTE;			// 0 to 255
typedef signed short int BYTE2;		// -32768 to 32767
typedef struct { int x, y; } xy;
