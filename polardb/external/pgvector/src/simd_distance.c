#include "postgres.h"
#include "simd_distance.h"
#include <math.h>

/* CPU feature detection flags */
int simd_support_avx512 = 0;
int simd_support_avx2 = 0;

/* Include SIMD intrinsics */
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#if defined(__AVX512F__)
#include <immintrin.h>
#define HAS_AVX512
#endif
#if defined(__AVX2__)
#include <immintrin.h>
#define HAS_AVX2
#endif
#if defined(__AVX__)
#include <immintrin.h>
#endif
#if defined(__SSE2__)
#include <emmintrin.h>
#endif
#endif

/*
 * Detect CPU SIMD capabilities using CPUID
 */
void simd_init(void)
{
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#if defined(__GNUC__) || defined(__clang__)
	unsigned int eax, ebx, ecx, edx;
	unsigned int xcr0_lo, xcr0_hi;

	/* Check for AVX2 support (CPUID EAX=7, ECX=0: EBX bit 5) */
	__asm__ __volatile__(
		"cpuid"
		: "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
		: "a"(7), "c"(0)
	);
	simd_support_avx2 = (ebx & (1 << 5)) ? 1 : 0;

	/* Check for AVX-512F support (CPUID EAX=7, ECX=0: EBX bit 16) */
	simd_support_avx512 = (ebx & (1 << 16)) ? 1 : 0;

	/* Verify OS support for AVX-512 via XGETBV */
	if (simd_support_avx512)
	{
		__asm__ __volatile__(
			"xgetbv"
			: "=a"(xcr0_lo), "=d"(xcr0_hi)
			: "c"(0)
		);
		/* Check bits 7:5 for ZMM state (AVX-512) */
		if ((xcr0_lo & 0xE6) != 0xE6)
		{
			simd_support_avx512 = 0;
		}
	}
#endif
#endif
}

/*
 * AVX-512 optimized L2 squared distance
 */
#ifdef HAS_AVX512
__attribute__((target("avx512f")))
static inline float
avx512_l2_squared_distance(int dim, const float *ax, const float *bx)
{
	__m512 sum;
	__m512 a, b, diff;
	float result;
	int i;

	sum = _mm512_setzero_ps();
	i = 0;

	/* Process 16 floats at a time */
	for (; i + 15 < dim; i += 16)
	{
		a = _mm512_loadu_ps(&ax[i]);
		b = _mm512_loadu_ps(&bx[i]);
		diff = _mm512_sub_ps(a, b);
		sum = _mm512_fmadd_ps(diff, diff, sum);
	}

	/* Horizontal sum */
	result = _mm512_reduce_add_ps(sum);

	/* Handle remaining elements */
	for (; i < dim; i++)
	{
		float d = ax[i] - bx[i];
		result += d * d;
	}

	return result;
}
#endif

/*
 * AVX2 optimized L2 squared distance
 */
#ifdef HAS_AVX2
__attribute__((target("avx2,fma")))
static inline float
avx2_l2_squared_distance(int dim, const float *ax, const float *bx)
{
	__m256 sum, a, b, diff;
	__m128 sum_high, sum_low, sum128;
	float result;
	int i;

	sum = _mm256_setzero_ps();
	i = 0;

	/* Process 8 floats at a time */
	for (; i + 7 < dim; i += 8)
	{
		a = _mm256_loadu_ps(&ax[i]);
		b = _mm256_loadu_ps(&bx[i]);
		diff = _mm256_sub_ps(a, b);
		sum = _mm256_fmadd_ps(diff, diff, sum);
	}

	/* Horizontal sum */
	sum_high = _mm256_extractf128_ps(sum, 1);
	sum_low = _mm256_castps256_ps128(sum);
	sum128 = _mm_add_ps(sum_low, sum_high);
	sum128 = _mm_hadd_ps(sum128, sum128);
	sum128 = _mm_hadd_ps(sum128, sum128);
	result = _mm_cvtss_f32(sum128);

	/* Handle remaining elements */
	for (; i < dim; i++)
	{
		float d = ax[i] - bx[i];
		result += d * d;
	}

	return result;
}
#endif

/*
 * Fallback L2 squared distance
 */
static inline float
fallback_l2_squared_distance(int dim, const float *ax, const float *bx)
{
	float distance;
	int i;

	distance = 0.0f;

	for (i = 0; i < dim; i++)
	{
		float diff = ax[i] - bx[i];
		distance += diff * diff;
	}

	return distance;
}

/*
 * Public L2 squared distance function
 */
float
simd_l2_squared_distance(int dim, const float *ax, const float *bx)
{
#ifdef HAS_AVX512
	if (simd_support_avx512)
		return avx512_l2_squared_distance(dim, ax, bx);
#endif
#ifdef HAS_AVX2
	if (simd_support_avx2)
		return avx2_l2_squared_distance(dim, ax, bx);
#endif
	return fallback_l2_squared_distance(dim, ax, bx);
}

/*
 * AVX-512 optimized inner product
 */
#ifdef HAS_AVX512
__attribute__((target("avx512f")))
static inline float
avx512_inner_product(int dim, const float *ax, const float *bx)
{
	__m512 sum, a, b;
	float result;
	int i;

	sum = _mm512_setzero_ps();
	i = 0;

	/* Process 16 floats at a time */
	for (; i + 15 < dim; i += 16)
	{
		a = _mm512_loadu_ps(&ax[i]);
		b = _mm512_loadu_ps(&bx[i]);
		sum = _mm512_fmadd_ps(a, b, sum);
	}

	/* Horizontal sum */
	result = _mm512_reduce_add_ps(sum);

	/* Handle remaining elements */
	for (; i < dim; i++)
	{
		result += ax[i] * bx[i];
	}

	return result;
}
#endif

/*
 * AVX2 optimized inner product
 */
#ifdef HAS_AVX2
__attribute__((target("avx2,fma")))
static inline float
avx2_inner_product(int dim, const float *ax, const float *bx)
{
	__m256 sum, a, b;
	__m128 sum_high, sum_low, sum128;
	float result;
	int i;

	sum = _mm256_setzero_ps();
	i = 0;

	/* Process 8 floats at a time */
	for (; i + 7 < dim; i += 8)
	{
		a = _mm256_loadu_ps(&ax[i]);
		b = _mm256_loadu_ps(&bx[i]);
		sum = _mm256_fmadd_ps(a, b, sum);
	}

	/* Horizontal sum */
	sum_high = _mm256_extractf128_ps(sum, 1);
	sum_low = _mm256_castps256_ps128(sum);
	sum128 = _mm_add_ps(sum_low, sum_high);
	sum128 = _mm_hadd_ps(sum128, sum128);
	sum128 = _mm_hadd_ps(sum128, sum128);
	result = _mm_cvtss_f32(sum128);

	/* Handle remaining elements */
	for (; i < dim; i++)
	{
		result += ax[i] * bx[i];
	}

	return result;
}
#endif

/*
 * Fallback inner product
 */
static inline float
fallback_inner_product(int dim, const float *ax, const float *bx)
{
	float result;
	int i;

	result = 0.0f;

	for (i = 0; i < dim; i++)
	{
		result += ax[i] * bx[i];
	}

	return result;
}

/*
 * Public inner product function
 */
float
simd_inner_product(int dim, const float *ax, const float *bx)
{
#ifdef HAS_AVX512
	if (simd_support_avx512)
		return avx512_inner_product(dim, ax, bx);
#endif
#ifdef HAS_AVX2
	if (simd_support_avx2)
		return avx2_inner_product(dim, ax, bx);
#endif
	return fallback_inner_product(dim, ax, bx);
}

/*
 * AVX-512 optimized cosine similarity
 */
#ifdef HAS_AVX512
__attribute__((target("avx512f")))
static inline double
avx512_cosine_similarity(int dim, const float *ax, const float *bx)
{
	__m512 dot_sum, norm_a_sum, norm_b_sum;
	__m512 a, b;
	float dot, norm_a, norm_b;
	int i;

	dot_sum = _mm512_setzero_ps();
	norm_a_sum = _mm512_setzero_ps();
	norm_b_sum = _mm512_setzero_ps();
	i = 0;

	/* Process 16 floats at a time */
	for (; i + 15 < dim; i += 16)
	{
		a = _mm512_loadu_ps(&ax[i]);
		b = _mm512_loadu_ps(&bx[i]);
		dot_sum = _mm512_fmadd_ps(a, b, dot_sum);
		norm_a_sum = _mm512_fmadd_ps(a, a, norm_a_sum);
		norm_b_sum = _mm512_fmadd_ps(b, b, norm_b_sum);
	}

	/* Horizontal sums */
	dot = _mm512_reduce_add_ps(dot_sum);
	norm_a = _mm512_reduce_add_ps(norm_a_sum);
	norm_b = _mm512_reduce_add_ps(norm_b_sum);

	/* Handle remaining elements */
	for (; i < dim; i++)
	{
		dot += ax[i] * bx[i];
		norm_a += ax[i] * ax[i];
		norm_b += bx[i] * bx[i];
	}

	return (double)dot / sqrt((double)norm_a * (double)norm_b);
}
#endif

/*
 * AVX2 optimized cosine similarity
 */
#ifdef HAS_AVX2
__attribute__((target("avx2,fma")))
static inline double
avx2_cosine_similarity(int dim, const float *ax, const float *bx)
{
	__m256 dot_sum, norm_a_sum, norm_b_sum;
	__m256 a, b;
	__m128 dot_high, dot_low, dot128;
	__m128 norm_a_high, norm_a_low, norm_a128;
	__m128 norm_b_high, norm_b_low, norm_b128;
	float dot, norm_a, norm_b;
	int i;

	dot_sum = _mm256_setzero_ps();
	norm_a_sum = _mm256_setzero_ps();
	norm_b_sum = _mm256_setzero_ps();
	i = 0;

	/* Process 8 floats at a time */
	for (; i + 7 < dim; i += 8)
	{
		a = _mm256_loadu_ps(&ax[i]);
		b = _mm256_loadu_ps(&bx[i]);
		dot_sum = _mm256_fmadd_ps(a, b, dot_sum);
		norm_a_sum = _mm256_fmadd_ps(a, a, norm_a_sum);
		norm_b_sum = _mm256_fmadd_ps(b, b, norm_b_sum);
	}

	/* Horizontal sums */
	dot_high = _mm256_extractf128_ps(dot_sum, 1);
	dot_low = _mm256_castps256_ps128(dot_sum);
	dot128 = _mm_add_ps(dot_low, dot_high);
	dot128 = _mm_hadd_ps(dot128, dot128);
	dot128 = _mm_hadd_ps(dot128, dot128);
	dot = _mm_cvtss_f32(dot128);

	norm_a_high = _mm256_extractf128_ps(norm_a_sum, 1);
	norm_a_low = _mm256_castps256_ps128(norm_a_sum);
	norm_a128 = _mm_add_ps(norm_a_low, norm_a_high);
	norm_a128 = _mm_hadd_ps(norm_a128, norm_a128);
	norm_a128 = _mm_hadd_ps(norm_a128, norm_a128);
	norm_a = _mm_cvtss_f32(norm_a128);

	norm_b_high = _mm256_extractf128_ps(norm_b_sum, 1);
	norm_b_low = _mm256_castps256_ps128(norm_b_sum);
	norm_b128 = _mm_add_ps(norm_b_low, norm_b_high);
	norm_b128 = _mm_hadd_ps(norm_b128, norm_b128);
	norm_b128 = _mm_hadd_ps(norm_b128, norm_b128);
	norm_b = _mm_cvtss_f32(norm_b128);

	/* Handle remaining elements */
	for (; i < dim; i++)
	{
		dot += ax[i] * bx[i];
		norm_a += ax[i] * ax[i];
		norm_b += bx[i] * bx[i];
	}

	return (double)dot / sqrt((double)norm_a * (double)norm_b);
}
#endif

/*
 * Fallback cosine similarity
 */
static inline double
fallback_cosine_similarity(int dim, const float *ax, const float *bx)
{
	float dot, norm_a, norm_b;
	int i;

	dot = 0.0f;
	norm_a = 0.0f;
	norm_b = 0.0f;

	for (i = 0; i < dim; i++)
	{
		dot += ax[i] * bx[i];
		norm_a += ax[i] * ax[i];
		norm_b += bx[i] * bx[i];
	}

	return (double)dot / sqrt((double)norm_a * (double)norm_b);
}

/*
 * Public cosine similarity function
 */
double
simd_cosine_similarity(int dim, const float *ax, const float *bx)
{
#ifdef HAS_AVX512
	if (simd_support_avx512)
		return avx512_cosine_similarity(dim, ax, bx);
#endif
#ifdef HAS_AVX2
	if (simd_support_avx2)
		return avx2_cosine_similarity(dim, ax, bx);
#endif
	return fallback_cosine_similarity(dim, ax, bx);
}

/*
 * AVX-512 optimized L1 distance
 */
#ifdef HAS_AVX512
__attribute__((target("avx512f")))
static inline float
avx512_l1_distance(int dim, const float *ax, const float *bx)
{
	__m512 sum, sign_mask;
	__m512 a, b, diff, abs_diff;
	float result;
	int i;

	sum = _mm512_setzero_ps();
	sign_mask = _mm512_set1_ps(-0.0f);
	i = 0;

	/* Process 16 floats at a time */
	for (; i + 15 < dim; i += 16)
	{
		a = _mm512_loadu_ps(&ax[i]);
		b = _mm512_loadu_ps(&bx[i]);
		diff = _mm512_sub_ps(a, b);
		/* Compute absolute value by clearing sign bit */
		abs_diff = _mm512_andnot_ps(sign_mask, diff);
		sum = _mm512_add_ps(sum, abs_diff);
	}

	/* Horizontal sum */
	result = _mm512_reduce_add_ps(sum);

	/* Handle remaining elements */
	for (; i < dim; i++)
	{
		result += fabsf(ax[i] - bx[i]);
	}

	return result;
}
#endif

/*
 * AVX2 optimized L1 distance
 */
#ifdef HAS_AVX2
__attribute__((target("avx2")))
static inline float
avx2_l1_distance(int dim, const float *ax, const float *bx)
{
	__m256 sum, sign_mask;
	__m256 a, b, diff, abs_diff;
	__m128 sum_high, sum_low, sum128;
	float result;
	int i;

	sum = _mm256_setzero_ps();
	sign_mask = _mm256_set1_ps(-0.0f);
	i = 0;

	/* Process 8 floats at a time */
	for (; i + 7 < dim; i += 8)
	{
		a = _mm256_loadu_ps(&ax[i]);
		b = _mm256_loadu_ps(&bx[i]);
		diff = _mm256_sub_ps(a, b);
		/* Compute absolute value by clearing sign bit */
		abs_diff = _mm256_andnot_ps(sign_mask, diff);
		sum = _mm256_add_ps(sum, abs_diff);
	}

	/* Horizontal sum */
	sum_high = _mm256_extractf128_ps(sum, 1);
	sum_low = _mm256_castps256_ps128(sum);
	sum128 = _mm_add_ps(sum_low, sum_high);
	sum128 = _mm_hadd_ps(sum128, sum128);
	sum128 = _mm_hadd_ps(sum128, sum128);
	result = _mm_cvtss_f32(sum128);

	/* Handle remaining elements */
	for (; i < dim; i++)
	{
		result += fabsf(ax[i] - bx[i]);
	}

	return result;
}
#endif

/*
 * Fallback L1 distance
 */
static inline float
fallback_l1_distance(int dim, const float *ax, const float *bx)
{
	float distance;
	int i;

	distance = 0.0f;

	for (i = 0; i < dim; i++)
	{
		distance += fabsf(ax[i] - bx[i]);
	}

	return distance;
}

/*
 * Public L1 distance function
 */
float
simd_l1_distance(int dim, const float *ax, const float *bx)
{
#ifdef HAS_AVX512
	if (simd_support_avx512)
		return avx512_l1_distance(dim, ax, bx);
#endif
#ifdef HAS_AVX2
	if (simd_support_avx2)
		return avx2_l1_distance(dim, ax, bx);
#endif
	return fallback_l1_distance(dim, ax, bx);
}
