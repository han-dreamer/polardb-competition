#ifndef SIMD_DIST_H
#define SIMD_DIST_H

#include <stdint.h>

/* Initialize runtime dispatch (detect CPU features). Safe to call multiple times. */
void SimdInit(void);

/* Compute L2 squared distance between two float vectors (dim > 0). Returns float. */
float SimdL2SquaredDistance(int dim, const float *a, const float *b);

/* Compute inner product (dot) between two float vectors. */
float SimdInnerProduct(int dim, const float *a, const float *b);

#endif /* SIMD_DIST_H */

