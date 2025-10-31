#include "simd_dist.h"
#include <immintrin.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

/* Function pointer types */
typedef float (*dist_fn_t)(int, const float *, const float *);

/* Forward declarations for implementations */
static float scalar_l2(int dim, const float *a, const float *b);
static float scalar_dot(int dim, const float *a, const float *b);
static float avx2_l2(int dim, const float *a, const float *b);
static float avx2_dot(int dim, const float *a, const float *b);

/* Runtime-selected implementations */
static dist_fn_t l2_impl = scalar_l2;
static dist_fn_t dot_impl = scalar_dot;
static bool simd_initialized = false;

void
SimdInit(void)
{
    if (simd_initialized)
        return;

#if defined(__GNUC__) || defined(__clang__)
    /* Runtime CPU feature detection */
    if (__builtin_cpu_supports("avx2"))
    {
        l2_impl = avx2_l2;
        dot_impl = avx2_dot;
    }
    else
    {
        l2_impl = scalar_l2;
        dot_impl = scalar_dot;
    }
#else
    /* Fallback: no runtime detection available */
    l2_impl = scalar_l2;
    dot_impl = scalar_dot;
#endif

    simd_initialized = true;
}

float
SimdL2SquaredDistance(int dim, const float *a, const float *b)
{
    return l2_impl(dim, a, b);
}

float
SimdInnerProduct(int dim, const float *a, const float *b)
{
    return dot_impl(dim, a, b);
}

/* Scalar implementations */
static float
scalar_l2(int dim, const float *a, const float *b)
{
    float sum = 0.0f;
    for (int i = 0; i < dim; i++)
    {
        float d = a[i] - b[i];
        sum += d * d;
    }
    return sum;
}

static float
scalar_dot(int dim, const float *a, const float *b)
{
    float sum = 0.0f;
    for (int i = 0; i < dim; i++)
        sum += a[i] * b[i];
    return sum;
}

/* AVX2 implementations (handles tail) */
static float
avx2_l2(int dim, const float *a, const float *b)
{
    int i = 0;
    __m256 vsum = _mm256_setzero_ps();

    for (; i + 7 < dim; i += 8)
    {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        __m256 v = _mm256_sub_ps(va, vb);
#if defined(__FMA__)
        vsum = _mm256_fmadd_ps(v, v, vsum);
#else
        __m256 prod = _mm256_mul_ps(v, v);
        vsum = _mm256_add_ps(vsum, prod);
#endif
    }

    float buf[8];
    _mm256_storeu_ps(buf, vsum);
    float sum = buf[0] + buf[1] + buf[2] + buf[3] + buf[4] + buf[5] + buf[6] + buf[7];

    for (; i < dim; i++)
    {
        float d = a[i] - b[i];
        sum += d * d;
    }

    return sum;
}

static float
avx2_dot(int dim, const float *a, const float *b)
{
    int i = 0;
    __m256 vsum = _mm256_setzero_ps();

    for (; i + 7 < dim; i += 8)
    {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
#if defined(__FMA__)
        vsum = _mm256_fmadd_ps(va, vb, vsum);
#else
        __m256 prod = _mm256_mul_ps(va, vb);
        vsum = _mm256_add_ps(vsum, prod);
#endif
    }

    float buf[8];
    _mm256_storeu_ps(buf, vsum);
    float sum = buf[0] + buf[1] + buf[2] + buf[3] + buf[4] + buf[5] + buf[6] + buf[7];

    for (; i < dim; i++)
        sum += a[i] * b[i];

    return sum;
}

