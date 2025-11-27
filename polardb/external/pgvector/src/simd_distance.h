#ifndef SIMD_DISTANCE_H
#define SIMD_DISTANCE_H

#include <stdint.h>

/*
 * SIMD-optimized distance calculation functions for pgvector
 * Supports AVX-512, AVX2, and fallback implementations
 */

/* Feature detection flags */
extern int simd_support_avx512;
extern int simd_support_avx2;

/* Initialize SIMD support detection */
void simd_init(void);

/* SIMD-optimized L2 squared distance */
float simd_l2_squared_distance(int dim, const float *ax, const float *bx);

/* SIMD-optimized inner product */
float simd_inner_product(int dim, const float *ax, const float *bx);

/* SIMD-optimized cosine similarity */
double simd_cosine_similarity(int dim, const float *ax, const float *bx);

/* SIMD-optimized L1 distance */
float simd_l1_distance(int dim, const float *ax, const float *bx);

#endif /* SIMD_DISTANCE_H */
