/**
 * @file dashem.c
 * @brief Core implementation of the em-dash removal library
 *
 * This file implements high-performance string processing for removing
 * em-dashes (U+2014) using multiple SIMD backends with automatic dispatch.
 */

#include "dashem.h"

#include <string.h>
#include <stdio.h>

/* ============================================================================
 * CPU Feature Detection
 * ============================================================================ */

#if defined(__GNUC__) || defined(__clang__)
    #include <cpuid.h>

static uint32_t __detect_cpu_features(void) {
    uint32_t features = DASHEM_CPU_SCALAR;
    uint32_t eax, ebx, ecx, edx;

    /* Check for SSE2 */
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        if (edx & (1U << 26)) features |= DASHEM_CPU_SSE2;
        if (ecx & (1U << 0))  features |= DASHEM_CPU_SSE42;
        if (ecx & (1U << 28)) features |= DASHEM_CPU_AVX;
    }

    /* Check for AVX2 and AVX-512 */
    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
        if (ebx & (1U << 5))  features |= DASHEM_CPU_AVX2;
        if (ebx & (1U << 16)) features |= DASHEM_CPU_AVX512F;
    }

    return features;
}

#elif defined(_MSC_VER)
    #include <intrin.h>

static uint32_t __detect_cpu_features(void) {
    uint32_t features = DASHEM_CPU_SCALAR;
    int cpuid_info[4] = {0};

    /* Check for SSE2 and AVX */
    __cpuid(cpuid_info, 1);
    if (cpuid_info[3] & (1U << 26)) features |= DASHEM_CPU_SSE2;
    if (cpuid_info[2] & (1U << 0))  features |= DASHEM_CPU_SSE42;
    if (cpuid_info[2] & (1U << 28)) features |= DASHEM_CPU_AVX;

    /* Check for AVX2 and AVX-512 */
    __cpuidex(cpuid_info, 7, 0);
    if (cpuid_info[1] & (1U << 5))  features |= DASHEM_CPU_AVX2;
    if (cpuid_info[1] & (1U << 16)) features |= DASHEM_CPU_AVX512F;

    return features;
}

#else
static uint32_t __detect_cpu_features(void) {
    return DASHEM_CPU_SCALAR;
}
#endif

/* Global state for CPU feature detection */
static uint32_t g_cpu_features = 0;
static int g_features_detected = 0;

uint32_t dashem_detect_cpu_features(void) {
    if (!g_features_detected) {
        g_cpu_features = __detect_cpu_features();
        g_features_detected = 1;
    }
    return g_cpu_features;
}

/* ============================================================================
 * Scalar Implementation (Portable Fallback)
 * ============================================================================ */

static int dashem_remove_scalar(
    const char *input,
    size_t input_len,
    char *output,
    size_t output_capacity,
    size_t *output_len
) {
    if (output_capacity < input_len) {
        return -1;
    }

    size_t out_idx = 0;

    for (size_t i = 0; i < input_len; ) {
        /* Check for em-dash (UTF-8: 0xE2 0x80 0x94) */
        if (i + 2 < input_len &&
            (unsigned char)input[i] == DASHEM_EM_DASH_BYTE1 &&
            (unsigned char)input[i + 1] == DASHEM_EM_DASH_BYTE2 &&
            (unsigned char)input[i + 2] == DASHEM_EM_DASH_BYTE3) {
            /* Skip em-dash (3 bytes) */
            i += 3;
        } else {
            /* Copy byte */
            output[out_idx++] = input[i++];
        }
    }

    *output_len = out_idx;
    return 0;
}

/* ============================================================================
 * SIMD Implementation - AVX2
 * ============================================================================ */

#if defined(__AVX2__) || (defined(_MSC_VER) && defined(__AVX2__))
    #include <immintrin.h>

static int dashem_remove_avx2(
    const char *input,
    size_t input_len,
    char *output,
    size_t output_capacity,
    size_t *output_len
) {
    if (output_capacity < input_len) {
        return -1;
    }

    size_t out_idx = 0;
    size_t i = 0;

    /* Process 32 bytes at a time */
    while (i + 32 <= input_len) {
        __m256i v = _mm256_loadu_si256((__m256i *)(input + i));

        /* Check for first byte of em-dash (0xE2) */
        __m256i cmp = _mm256_cmpeq_epi8(v, _mm256_set1_epi8(0xE2));

        /* Convert to mask */
        uint32_t mask = _mm256_movemask_epi8(cmp);

        if (mask == 0) {
            /* No potential em-dashes in this chunk, copy directly */
            memcpy(output + out_idx, input + i, 32);
            out_idx += 32;
            i += 32;
        } else {
            /* Process byte by byte when we detect potential em-dashes */
            for (int j = 0; j < 32 && i < input_len; ) {
                if (i + 2 < input_len &&
                    (unsigned char)input[i] == DASHEM_EM_DASH_BYTE1 &&
                    (unsigned char)input[i + 1] == DASHEM_EM_DASH_BYTE2 &&
                    (unsigned char)input[i + 2] == DASHEM_EM_DASH_BYTE3) {
                    i += 3;
                    j += 3;
                } else {
                    output[out_idx++] = input[i++];
                    j++;
                }
            }
        }
    }

    /* Process remainder with scalar */
    while (i < input_len) {
        if (i + 2 < input_len &&
            (unsigned char)input[i] == DASHEM_EM_DASH_BYTE1 &&
            (unsigned char)input[i + 1] == DASHEM_EM_DASH_BYTE2 &&
            (unsigned char)input[i + 2] == DASHEM_EM_DASH_BYTE3) {
            i += 3;
        } else {
            output[out_idx++] = input[i++];
        }
    }

    *output_len = out_idx;
    return 0;
}
#endif

/* ============================================================================
 * SIMD Implementation - SSE4.2
 * ============================================================================ */

#if defined(__SSE4_2__) || (defined(_MSC_VER) && defined(__SSE4_2__))
    #include <nmmintrin.h>

static int dashem_remove_sse42(
    const char *input,
    size_t input_len,
    char *output,
    size_t output_capacity,
    size_t *output_len
) {
    if (output_capacity < input_len) {
        return -1;
    }

    size_t out_idx = 0;
    size_t i = 0;

    /* Process 16 bytes at a time */
    while (i + 16 <= input_len) {
        __m128i v = _mm_loadu_si128((__m128i *)(input + i));

        /* Check for first byte of em-dash (0xE2) */
        __m128i cmp = _mm_cmpeq_epi8(v, _mm_set1_epi8(0xE2));

        /* Convert to mask */
        uint32_t mask = _mm_movemask_epi8(cmp);

        if (mask == 0) {
            /* No potential em-dashes, copy directly */
            memcpy(output + out_idx, input + i, 16);
            out_idx += 16;
            i += 16;
        } else {
            /* Process byte by byte */
            for (int j = 0; j < 16 && i < input_len; ) {
                if (i + 2 < input_len &&
                    (unsigned char)input[i] == DASHEM_EM_DASH_BYTE1 &&
                    (unsigned char)input[i + 1] == DASHEM_EM_DASH_BYTE2 &&
                    (unsigned char)input[i + 2] == DASHEM_EM_DASH_BYTE3) {
                    i += 3;
                    j += 3;
                } else {
                    output[out_idx++] = input[i++];
                    j++;
                }
            }
        }
    }

    /* Process remainder */
    while (i < input_len) {
        if (i + 2 < input_len &&
            (unsigned char)input[i] == DASHEM_EM_DASH_BYTE1 &&
            (unsigned char)input[i + 1] == DASHEM_EM_DASH_BYTE2 &&
            (unsigned char)input[i + 2] == DASHEM_EM_DASH_BYTE3) {
            i += 3;
        } else {
            output[out_idx++] = input[i++];
        }
    }

    *output_len = out_idx;
    return 0;
}
#endif

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

int dashem_remove(
    const char *input,
    size_t input_len,
    char *output,
    size_t output_capacity,
    size_t *output_len
) {
    if (!input || !output || !output_len) {
        return -2;
    }

    /* Detect CPU features if not already done */
    uint32_t features = dashem_detect_cpu_features();

    /* Dispatch to optimal implementation */
#if defined(__AVX2__)
    if (features & DASHEM_CPU_AVX2) {
        return dashem_remove_avx2(input, input_len, output, output_capacity, output_len);
    }
#endif

#if defined(__SSE4_2__)
    if (features & DASHEM_CPU_SSE42) {
        return dashem_remove_sse42(input, input_len, output, output_capacity, output_len);
    }
#endif

    /* Scalar fallback */
    return dashem_remove_scalar(input, input_len, output, output_capacity, output_len);
}

const char* dashem_version(void) {
    return "1.0.0";
}

const char* dashem_implementation_name(void) {
    uint32_t features = dashem_detect_cpu_features();

#if defined(__AVX2__)
    if (features & DASHEM_CPU_AVX2) {
        return "AVX2";
    }
#endif

#if defined(__SSE4_2__)
    if (features & DASHEM_CPU_SSE42) {
        return "SSE4.2";
    }
#endif

    return "Scalar";
}
