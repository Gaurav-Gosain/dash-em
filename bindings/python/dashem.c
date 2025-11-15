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

/* x86/x86_64 CPUID-based detection */
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86))
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

/* MSVC x86/x86_64 detection */
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
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

/* ARM/ARM64 with NEON detection */
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
static uint32_t __detect_cpu_features(void) {
    return DASHEM_CPU_SCALAR | DASHEM_CPU_NEON;
}

/* Fallback for unknown architectures */
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
    const unsigned char *in_ptr = (const unsigned char *)input;
    unsigned char *out_ptr = (unsigned char *)output;

    /* Process input with unrolled loops to reduce branch overhead */
    size_t i = 0;

    /* Fast path: process 8 bytes at a time when no matches */
    while (i + 8 <= input_len) {
        /* Check if any byte could be the start of em-dash (0xE2) */
        uint64_t chunk = *(uint64_t *)(input + i);
        int has_e2 = 0;

        for (int j = 0; j < 8; j++) {
            if (((chunk >> (j * 8)) & 0xFF) == 0xE2) {
                has_e2 = 1;
                break;
            }
        }

        if (!has_e2) {
            /* No em-dash pattern start in this chunk, copy directly */
            memcpy(out_ptr + out_idx, input + i, 8);
            out_idx += 8;
            i += 8;
        } else {
            /* Process byte-by-byte */
            break;
        }
    }

    /* Process remaining bytes */
    while (i < input_len) {
        if (i + 2 < input_len &&
            in_ptr[i] == 0xE2 &&
            in_ptr[i + 1] == 0x80 &&
            in_ptr[i + 2] == 0x94) {
            /* Skip em-dash */
            i += 3;
        } else {
            out_ptr[out_idx++] = in_ptr[i++];
        }
    }

    *output_len = out_idx;
    return 0;
}

/* ============================================================================
 * SIMD Implementation - AVX2
 * ============================================================================ */

#if defined(__AVX2__)
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
    const unsigned char *in_ptr = (const unsigned char *)input;
    unsigned char *out_ptr = (unsigned char *)output;

    /* SSSE3 pshufb-based pattern matching for em-dash detection */
    /* Pattern: 0xE2 0x80 0x94 */

    /* Create lookup pattern for SSSE3 shuffle */
    const __m256i pattern_0xe2 = _mm256_set1_epi8(0xE2);

    /* Process 32 bytes at a time */
    while (i + 32 <= input_len) {
        __m256i v = _mm256_loadu_si256((__m256i *)(input + i));
        __m256i cmp = _mm256_cmpeq_epi8(v, pattern_0xe2);
        uint32_t mask = _mm256_movemask_epi8(cmp);

        /* Fast path: no 0xE2 bytes found, copy entire chunk */
        if (mask == 0) {
            memcpy(out_ptr + out_idx, input + i, 32);
            out_idx += 32;
            i += 32;
            continue;
        }

        /* Process each potential position */
        for (int j = 0; j < 32; j++) {
            if ((mask & (1 << j)) && i + j + 2 < input_len) {
                if (in_ptr[i + j] == 0xE2 && in_ptr[i + j + 1] == 0x80 && in_ptr[i + j + 2] == 0x94) {
                    /* Copy up to this point */
                    if (j > 0) {
                        memcpy(out_ptr + out_idx, input + i, j);
                        out_idx += j;
                    }
                    i += j + 3; /* Skip em-dash */
                    j = 32; /* Break inner loop */
                }
            }
        }

        if (i < 32 + (i & ~31)) {
            /* Copy remaining bytes from this chunk */
            size_t remaining = 32 - (i & 31);
            if (remaining > 0) {
                memcpy(out_ptr + out_idx, input + (i & ~31), remaining);
                out_idx += remaining;
            }
            i = (i & ~31) + 32;
        }
    }

    /* Process remainder with scalar */
    while (i < input_len) {
        if (i + 2 < input_len &&
            in_ptr[i] == 0xE2 &&
            in_ptr[i + 1] == 0x80 &&
            in_ptr[i + 2] == 0x94) {
            i += 3;
        } else {
            out_ptr[out_idx++] = in_ptr[i++];
        }
    }

    *output_len = out_idx;
    return 0;
}
#endif

/* ============================================================================
 * SIMD Implementation - SSE4.2
 * ============================================================================ */

#if defined(__SSE4_2__)
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
    const unsigned char *in_ptr = (const unsigned char *)input;
    unsigned char *out_ptr = (unsigned char *)output;

    const __m128i pattern_0xe2 = _mm_set1_epi8(0xE2);

    /* Process 16 bytes at a time */
    while (i + 16 <= input_len) {
        __m128i v = _mm_loadu_si128((__m128i *)(input + i));
        __m128i cmp = _mm_cmpeq_epi8(v, pattern_0xe2);
        uint32_t mask = _mm_movemask_epi8(cmp);

        if (mask == 0) {
            /* No 0xE2 bytes, copy chunk directly */
            memcpy(out_ptr + out_idx, input + i, 16);
            out_idx += 16;
            i += 16;
            continue;
        }

        /* Check each potential match */
        for (int j = 0; j < 16; j++) {
            if ((mask & (1 << j)) && i + j + 2 < input_len) {
                if (in_ptr[i + j] == 0xE2 && in_ptr[i + j + 1] == 0x80 && in_ptr[i + j + 2] == 0x94) {
                    if (j > 0) {
                        memcpy(out_ptr + out_idx, input + i, j);
                        out_idx += j;
                    }
                    i += j + 3;
                    j = 16; /* Break */
                }
            }
        }

        if (i < 16 + (i & ~15)) {
            size_t remaining = 16 - (i & 15);
            if (remaining > 0) {
                memcpy(out_ptr + out_idx, input + (i & ~15), remaining);
                out_idx += remaining;
            }
            i = (i & ~15) + 16;
        }
    }

    /* Process remainder */
    while (i < input_len) {
        if (i + 2 < input_len &&
            in_ptr[i] == 0xE2 &&
            in_ptr[i + 1] == 0x80 &&
            in_ptr[i + 2] == 0x94) {
            i += 3;
        } else {
            out_ptr[out_idx++] = in_ptr[i++];
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
