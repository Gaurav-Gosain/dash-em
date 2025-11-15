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
 * Portable CTZ (Count Trailing Zeros) Implementation
 * ============================================================================ */

/* Portable count trailing zeros - works on GCC, Clang, and MSVC */
#ifdef _MSC_VER
    #include <intrin.h>
    static inline int dashem_ctz(uint32_t v) {
        unsigned long r;
        _BitScanForward(&r, v);
        return (int)r;
    }
#elif defined(__GNUC__) || defined(__clang__)
    #define dashem_ctz(x) __builtin_ctz(x)
#else
    /* Software fallback for other compilers */
    static inline int dashem_ctz(uint32_t v) {
        if (v == 0) return 32;
        int count = 0;
        if ((v & 0xFFFF) == 0) { count += 16; v >>= 16; }
        if ((v & 0xFF) == 0) { count += 8; v >>= 8; }
        if ((v & 0xF) == 0) { count += 4; v >>= 4; }
        if ((v & 0x3) == 0) { count += 2; v >>= 2; }
        if ((v & 0x1) == 0) { count += 1; }
        return count;
    }
#endif

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

    /* Process input with SWAR (SIMD Within A Register) optimization */
    size_t i = 0;

    /* Fast path: process 8 bytes at a time when no matches using SWAR technique */
    while (i + 8 <= input_len) {
        /* Check if any byte is 0xE2 using SWAR bit manipulation */
        uint64_t chunk = *(uint64_t *)(input + i);

        /* SWAR technique: check for 0xE2 in any of 8 bytes in parallel */
        uint64_t test = chunk ^ 0xE2E2E2E2E2E2E2E2ULL;
        uint64_t has_zero = (test - 0x0101010101010101ULL) & ~test & 0x8080808080808080ULL;

        if (has_zero == 0) {
            /* No 0xE2 bytes in this chunk, copy directly */
            memcpy(out_ptr + out_idx, input + i, 8);
            out_idx += 8;
            i += 8;
        } else {
            /* Has potential em-dash start, process byte-by-byte */
            break;
        }
    }

    /* Process remaining bytes */
    while (i < input_len) {
        if (i + 3 <= input_len &&
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
 * Fast Path for Small Strings (< 32 bytes)
 * ============================================================================ */

/* Specialized fast path for small inputs that avoids SIMD overhead */
static inline int dashem_remove_fast_small(
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

    /* For small strings, direct byte-by-byte processing with aggressive inlining */
    while (i < input_len) {
        if (i + 3 <= input_len &&
            in_ptr[i] == 0xE2 &&
            in_ptr[i + 1] == 0x80 &&
            in_ptr[i + 2] == 0x94) {
            /* Skip em-dash (3 bytes) */
            i += 3;
        } else {
            /* Copy single byte */
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

    /* Create patterns for all 3 bytes of em-dash */
    const __m256i pattern_0xe2 = _mm256_set1_epi8(0xE2);
    const __m256i pattern_0x80 = _mm256_set1_epi8(0x80);
    const __m256i pattern_0x94 = _mm256_set1_epi8(0x94);

    /* Process 32 bytes at a time with SIMD */
    while (i + 32 <= input_len) {
        __m256i v0 = _mm256_loadu_si256((__m256i *)(input + i));

        /* Load next two bytes for verification (handle boundaries) */
        __m256i v1 = (i + 1 < input_len) ? _mm256_loadu_si256((__m256i *)(input + i + 1)) : _mm256_setzero_si256();
        __m256i v2 = (i + 2 < input_len) ? _mm256_loadu_si256((__m256i *)(input + i + 2)) : _mm256_setzero_si256();

        /* Check all 3 bytes in parallel */
        __m256i cmp0 = _mm256_cmpeq_epi8(v0, pattern_0xe2);
        __m256i cmp1 = _mm256_cmpeq_epi8(v1, pattern_0x80);
        __m256i cmp2 = _mm256_cmpeq_epi8(v2, pattern_0x94);

        /* All 3 must match for a complete em-dash pattern */
        __m256i full_match = _mm256_and_si256(cmp0, _mm256_and_si256(cmp1, cmp2));
        uint32_t em_dash_mask = _mm256_movemask_epi8(full_match);

        /* Fast path: no em-dashes in this chunk */
        if (em_dash_mask == 0) {
            memcpy(out_ptr + out_idx, input + i, 32);
            out_idx += 32;
            i += 32;
            continue;
        }

        /* Process all potential em-dashes in this chunk */
        size_t write_pos = i;  /* Track position we're copying from */
        size_t processed = 0;  /* Track how many bits we've processed in the mask */

        while (em_dash_mask != 0) {
            /* Find the next set bit position within remaining mask */
            int match_offset = dashem_ctz(em_dash_mask);  /* Position in remaining mask */
            size_t match_pos = i + processed + match_offset;

            /* Copy bytes before this match */
            if (match_pos > write_pos) {
                size_t copy_len = match_pos - write_pos;
                memcpy(out_ptr + out_idx, input + write_pos, copy_len);
                out_idx += copy_len;
            }

            /* Move past the em-dash (3 bytes) */
            write_pos = match_pos + 3;
            processed += match_offset + 3;

            /* Remove processed bits from mask and continue */
            em_dash_mask >>= (match_offset + 3);
        }

        /* Copy any remaining bytes from this chunk */
        size_t chunk_end = i + 32;
        if (write_pos < chunk_end) {
            size_t remaining = chunk_end - write_pos;
            memcpy(out_ptr + out_idx, input + write_pos, remaining);
            out_idx += remaining;
        }

        i = chunk_end;
    }

    /* Process remainder with scalar */
    while (i < input_len) {
        if (i + 3 <= input_len &&
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

/* 64-byte unrolled AVX2 variant for improved instruction-level parallelism */
static int dashem_remove_avx2_unrolled(
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

    /* Create patterns for all 3 bytes of em-dash */
    const __m256i pattern_0xe2 = _mm256_set1_epi8(0xE2);
    const __m256i pattern_0x80 = _mm256_set1_epi8(0x80);
    const __m256i pattern_0x94 = _mm256_set1_epi8(0x94);

    /* Process 64 bytes at a time (two 32-byte chunks) with unrolled loop */
    while (i + 64 <= input_len) {
        /* First 32-byte chunk */
        __m256i v0_a = _mm256_loadu_si256((__m256i *)(input + i));
        __m256i v1_a = _mm256_loadu_si256((__m256i *)(input + i + 1));
        __m256i v2_a = _mm256_loadu_si256((__m256i *)(input + i + 2));

        /* Second 32-byte chunk */
        __m256i v0_b = _mm256_loadu_si256((__m256i *)(input + i + 32));
        __m256i v1_b = _mm256_loadu_si256((__m256i *)(input + i + 33));
        __m256i v2_b = _mm256_loadu_si256((__m256i *)(input + i + 34));

        /* Compare first chunk */
        __m256i cmp0_a = _mm256_cmpeq_epi8(v0_a, pattern_0xe2);
        __m256i cmp1_a = _mm256_cmpeq_epi8(v1_a, pattern_0x80);
        __m256i cmp2_a = _mm256_cmpeq_epi8(v2_a, pattern_0x94);
        __m256i full_match_a = _mm256_and_si256(cmp0_a, _mm256_and_si256(cmp1_a, cmp2_a));
        uint32_t mask_a = _mm256_movemask_epi8(full_match_a);

        /* Compare second chunk */
        __m256i cmp0_b = _mm256_cmpeq_epi8(v0_b, pattern_0xe2);
        __m256i cmp1_b = _mm256_cmpeq_epi8(v1_b, pattern_0x80);
        __m256i cmp2_b = _mm256_cmpeq_epi8(v2_b, pattern_0x94);
        __m256i full_match_b = _mm256_and_si256(cmp0_b, _mm256_and_si256(cmp1_b, cmp2_b));
        uint32_t mask_b = _mm256_movemask_epi8(full_match_b);

        /* Fast path: no em-dashes in either chunk */
        if (mask_a == 0 && mask_b == 0) {
            memcpy(out_ptr + out_idx, input + i, 64);
            out_idx += 64;
            i += 64;
            continue;
        }

        /* Process first chunk if it has matches */
        size_t write_pos = i;
        if (mask_a != 0) {
            size_t processed = 0;
            while (mask_a != 0) {
                int match_offset = dashem_ctz(mask_a);
                size_t match_pos = i + processed + match_offset;

                if (match_pos > write_pos) {
                    size_t copy_len = match_pos - write_pos;
                    memcpy(out_ptr + out_idx, input + write_pos, copy_len);
                    out_idx += copy_len;
                }

                write_pos = match_pos + 3;
                processed += match_offset + 3;
                mask_a >>= (match_offset + 3);
            }

            size_t chunk_end = i + 32;
            if (write_pos < chunk_end) {
                size_t remaining = chunk_end - write_pos;
                memcpy(out_ptr + out_idx, input + write_pos, remaining);
                out_idx += remaining;
            }
            write_pos = i + 32;
        }

        /* Process second chunk if it has matches */
        if (mask_b != 0) {
            size_t processed = 0;
            while (mask_b != 0) {
                int match_offset = dashem_ctz(mask_b);
                size_t match_pos = i + 32 + processed + match_offset;

                if (match_pos > write_pos) {
                    size_t copy_len = match_pos - write_pos;
                    memcpy(out_ptr + out_idx, input + write_pos, copy_len);
                    out_idx += copy_len;
                }

                write_pos = match_pos + 3;
                processed += match_offset + 3;
                mask_b >>= (match_offset + 3);
            }

            size_t chunk_end = i + 64;
            if (write_pos < chunk_end) {
                size_t remaining = chunk_end - write_pos;
                memcpy(out_ptr + out_idx, input + write_pos, remaining);
                out_idx += remaining;
            }
        } else {
            /* No matches in second chunk, copy remaining bytes */
            size_t chunk_end = i + 64;
            if (write_pos < chunk_end) {
                size_t remaining = chunk_end - write_pos;
                memcpy(out_ptr + out_idx, input + write_pos, remaining);
                out_idx += remaining;
            }
        }

        i += 64;
    }

    /* Process remaining bytes with single 32-byte chunks */
    while (i + 32 <= input_len) {
        __m256i v0 = _mm256_loadu_si256((__m256i *)(input + i));
        __m256i v1 = _mm256_loadu_si256((__m256i *)(input + i + 1));
        __m256i v2 = _mm256_loadu_si256((__m256i *)(input + i + 2));

        __m256i cmp0 = _mm256_cmpeq_epi8(v0, pattern_0xe2);
        __m256i cmp1 = _mm256_cmpeq_epi8(v1, pattern_0x80);
        __m256i cmp2 = _mm256_cmpeq_epi8(v2, pattern_0x94);

        __m256i full_match = _mm256_and_si256(cmp0, _mm256_and_si256(cmp1, cmp2));
        uint32_t em_dash_mask = _mm256_movemask_epi8(full_match);

        if (em_dash_mask == 0) {
            memcpy(out_ptr + out_idx, input + i, 32);
            out_idx += 32;
            i += 32;
            continue;
        }

        size_t write_pos = i;
        size_t processed = 0;

        while (em_dash_mask != 0) {
            int match_offset = dashem_ctz(em_dash_mask);
            size_t match_pos = i + processed + match_offset;

            if (match_pos > write_pos) {
                size_t copy_len = match_pos - write_pos;
                memcpy(out_ptr + out_idx, input + write_pos, copy_len);
                out_idx += copy_len;
            }

            write_pos = match_pos + 3;
            processed += match_offset + 3;
            em_dash_mask >>= (match_offset + 3);
        }

        size_t chunk_end = i + 32;
        if (write_pos < chunk_end) {
            size_t remaining = chunk_end - write_pos;
            memcpy(out_ptr + out_idx, input + write_pos, remaining);
            out_idx += remaining;
        }

        i = chunk_end;
    }

    /* Process remainder with scalar */
    while (i < input_len) {
        if (i + 3 <= input_len &&
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

    /* Create patterns for all 3 bytes of em-dash */
    const __m128i pattern_0xe2 = _mm_set1_epi8(0xE2);
    const __m128i pattern_0x80 = _mm_set1_epi8(0x80);
    const __m128i pattern_0x94 = _mm_set1_epi8(0x94);

    /* Process 16 bytes at a time */
    while (i + 16 <= input_len) {
        __m128i v0 = _mm_loadu_si128((__m128i *)(input + i));
        __m128i v1 = (i + 1 < input_len) ? _mm_loadu_si128((__m128i *)(input + i + 1)) : _mm_setzero_si128();
        __m128i v2 = (i + 2 < input_len) ? _mm_loadu_si128((__m128i *)(input + i + 2)) : _mm_setzero_si128();

        /* Check all 3 bytes in parallel */
        __m128i cmp0 = _mm_cmpeq_epi8(v0, pattern_0xe2);
        __m128i cmp1 = _mm_cmpeq_epi8(v1, pattern_0x80);
        __m128i cmp2 = _mm_cmpeq_epi8(v2, pattern_0x94);

        /* All 3 must match for a complete em-dash pattern */
        __m128i full_match = _mm_and_si128(cmp0, _mm_and_si128(cmp1, cmp2));
        uint32_t em_dash_mask = _mm_movemask_epi8(full_match);

        /* Fast path: no em-dashes in this chunk */
        if (em_dash_mask == 0) {
            memcpy(out_ptr + out_idx, input + i, 16);
            out_idx += 16;
            i += 16;
            continue;
        }

        /* Process all potential em-dashes in this chunk */
        size_t write_pos = i;  /* Track position we're copying from */
        size_t processed = 0;  /* Track how many bits we've processed in the mask */

        while (em_dash_mask != 0) {
            /* Find the next set bit position within remaining mask */
            int match_offset = dashem_ctz(em_dash_mask);  /* Position in remaining mask */
            size_t match_pos = i + processed + match_offset;

            /* Copy bytes before this match */
            if (match_pos > write_pos) {
                size_t copy_len = match_pos - write_pos;
                memcpy(out_ptr + out_idx, input + write_pos, copy_len);
                out_idx += copy_len;
            }

            /* Move past the em-dash (3 bytes) */
            write_pos = match_pos + 3;
            processed += match_offset + 3;

            /* Remove processed bits from mask and continue */
            em_dash_mask >>= (match_offset + 3);
        }

        /* Copy any remaining bytes from this chunk */
        size_t chunk_end = i + 16;
        if (write_pos < chunk_end) {
            size_t remaining = chunk_end - write_pos;
            memcpy(out_ptr + out_idx, input + write_pos, remaining);
            out_idx += remaining;
        }

        i = chunk_end;
    }

    /* Process remainder */
    while (i < input_len) {
        if (i + 3 <= input_len &&
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

    /* Fast path for small inputs (< 32 bytes) - avoids SIMD overhead */
    if (input_len < 32) {
        return dashem_remove_fast_small(input, input_len, output, output_capacity, output_len);
    }

    /* Detect CPU features if not already done */
    uint32_t features = dashem_detect_cpu_features();

    /* Dispatch to optimal implementation */
#if defined(__AVX2__)
    if (features & DASHEM_CPU_AVX2) {
        /* Use unrolled version for larger inputs, regular version for smaller */
        if (input_len >= 64) {
            return dashem_remove_avx2_unrolled(input, input_len, output, output_capacity, output_len);
        } else {
            return dashem_remove_avx2(input, input_len, output, output_capacity, output_len);
        }
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
