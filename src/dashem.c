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

/* MSVC needs intrin.h for popcount intrinsics */
#if defined(_MSC_VER)
    #include <intrin.h>
#endif

/* ============================================================================
 * Branch Prediction Hints
 * ============================================================================ */

/* LIKELY/UNLIKELY macros for branch prediction hints */
#if defined(__GNUC__) || defined(__clang__)
    #define LIKELY(x)   __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define LIKELY(x)   (x)
    #define UNLIKELY(x) (x)
#endif

/* ============================================================================
 * Compiler Attribute Macros
 * ============================================================================ */

/* Define UNUSED macro for suppressing unused function warnings */
#if defined(_MSC_VER)
    /* MSVC doesn't support __attribute__ */
    #define DASHEM_UNUSED
    #define DASHEM_ALWAYS_INLINE __forceinline
    /* MSVC uses different intrinsic names for popcount */
    #define DASHEM_POPCOUNT(x) __popcnt(x)
    #define DASHEM_POPCOUNTLL(x) __popcnt64(x)
#else
    /* GCC/Clang: use attribute to suppress unused warning */
    #define DASHEM_UNUSED __attribute__((unused))
    #define DASHEM_ALWAYS_INLINE __attribute__((always_inline)) inline
    /* GCC/Clang: use builtins */
    #define DASHEM_POPCOUNT(x) __builtin_popcount(x)
    #define DASHEM_POPCOUNTLL(x) __builtin_popcountll(x)
#endif

/* ============================================================================
 * Compile-Time Validation (Static Asserts)
 * ============================================================================ */

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    /* C11 and later: use _Static_assert (preferred) */
    #define DASHEM_STATIC_ASSERT(cond, msg) _Static_assert((cond), msg)
#else
    /* Pre-C11 fallback: compile-time assertion with unique names */
    #define DASHEM_CONCAT_IMPL(a, b) a ## b
    #define DASHEM_CONCAT(a, b) DASHEM_CONCAT_IMPL(a, b)

    #if defined(_MSC_VER)
        /* MSVC doesn't support __attribute__ */
        #define DASHEM_STATIC_ASSERT(cond, msg) \
            typedef char DASHEM_CONCAT(dashem_sa_, __LINE__)[(cond) ? 1 : -1]
    #else
        /* GCC/Clang: use attribute to suppress unused warning */
        #define DASHEM_STATIC_ASSERT(cond, msg) \
            typedef char DASHEM_CONCAT(dashem_sa_, __LINE__)[(cond) ? 1 : -1] __attribute__((unused))
    #endif
#endif

/* Validate em-dash pattern bytes at compile time */
DASHEM_STATIC_ASSERT(DASHEM_EM_DASH_BYTE1 == 0xE2, "Em-dash byte 1 must be 0xE2");
DASHEM_STATIC_ASSERT(DASHEM_EM_DASH_BYTE2 == 0x80, "Em-dash byte 2 must be 0x80");
DASHEM_STATIC_ASSERT(DASHEM_EM_DASH_BYTE3 == 0x94, "Em-dash byte 3 must be 0x94");

/* Note: SIMD register size validation would require including immintrin.h at top-level,
 * which would impose unnecessary dependencies on non-SIMD code paths. Instead, we
 * trust that compiler intrinsics are correctly sized on target platforms. */

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
    static inline int dashem_ctzll(uint64_t v) {
        unsigned long r;
    #if defined(_M_X64) || defined(_M_ARM64)
        _BitScanForward64(&r, v);
    #else
        if ((uint32_t)v != 0) {
            _BitScanForward(&r, (uint32_t)v);
        } else {
            _BitScanForward(&r, (uint32_t)(v >> 32));
            r += 32;
        }
    #endif
        return (int)r;
    }
#elif defined(__GNUC__) || defined(__clang__)
    #define dashem_ctz(x) __builtin_ctz(x)
    #define dashem_ctzll(x) __builtin_ctzll(x)
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
    static inline int dashem_ctzll(uint64_t v) {
        if (v == 0) return 64;
        if ((uint32_t)v != 0) return dashem_ctz((uint32_t)v);
        return 32 + dashem_ctz((uint32_t)(v >> 32));
    }
#endif

/* ============================================================================
 * Kernel Availability
 * ============================================================================ */

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
    #define DASHEM_X86 1
#endif

/* The x86 kernels are compiled for their own instruction set through target
 * attributes (GCC/Clang) or plain intrinsics (MSVC) and picked at runtime, so a
 * build for the generic x86-64 baseline (Python wheels, Rust, Go) still gets
 * SIMD. Define DASHEM_SCALAR_ONLY to build only the portable kernels. */
#if defined(DASHEM_X86) && !defined(DASHEM_SCALAR_ONLY)
    #if defined(__clang__) || defined(__GNUC__)
        #define DASHEM_X86_KERNELS 1
        #define DASHEM_TARGET(isa) __attribute__((target(isa)))
        #if (defined(__clang__) && __clang_major__ >= 10) || (!defined(__clang__) && __GNUC__ >= 8)
            #define DASHEM_AVX512_COMPILER 1
        #endif
    #elif defined(_MSC_VER)
        #define DASHEM_X86_KERNELS 1
        #define DASHEM_TARGET(isa)
        #if _MSC_VER >= 1920
            #define DASHEM_AVX512_COMPILER 1
        #endif
    #endif
    /* The AVX-512 kernel counts its 64-bit keep mask with a 64-bit popcount. */
    #if defined(DASHEM_AVX512_COMPILER) && (defined(__x86_64__) || defined(_M_X64))
        #define DASHEM_X86_AVX512 1
    #endif
#endif

#if defined(DASHEM_X86_KERNELS)
    #include <immintrin.h>
#endif

/* ============================================================================
 * CPU Feature Detection
 * ============================================================================ */

#if defined(DASHEM_X86) && (defined(__GNUC__) || defined(__clang__))
    #include <cpuid.h>

static void dashem_cpuid(uint32_t leaf, uint32_t subleaf, uint32_t regs[4]) {
    __cpuid_count(leaf, subleaf, regs[0], regs[1], regs[2], regs[3]);
}

/* XCR0: which register states the OS saves on a context switch. */
static uint64_t dashem_xgetbv(void) {
    uint32_t lo, hi;
    __asm__ __volatile__(".byte 0x0f, 0x01, 0xd0" : "=a"(lo), "=d"(hi) : "c"(0));
    return ((uint64_t)hi << 32) | lo;
}
    #define DASHEM_HAVE_CPUID 1

#elif defined(DASHEM_X86) && defined(_MSC_VER)
    #include <intrin.h>

static void dashem_cpuid(uint32_t leaf, uint32_t subleaf, uint32_t regs[4]) {
    int r[4];
    __cpuidex(r, (int)leaf, (int)subleaf);
    regs[0] = (uint32_t)r[0];
    regs[1] = (uint32_t)r[1];
    regs[2] = (uint32_t)r[2];
    regs[3] = (uint32_t)r[3];
}

static uint64_t dashem_xgetbv(void) {
    return (uint64_t)_xgetbv(0);
}
    #define DASHEM_HAVE_CPUID 1
#endif

#if defined(DASHEM_HAVE_CPUID)
static uint32_t __detect_cpu_features(void) {
    uint32_t features = DASHEM_CPU_SCALAR;
    uint32_t r[4];

    dashem_cpuid(0, 0, r);
    uint32_t max_leaf = r[0];
    if (max_leaf < 1) {
        return features;
    }

    dashem_cpuid(1, 0, r);
    uint32_t ecx1 = r[2];
    uint32_t edx1 = r[3];

    if (edx1 & (1U << 26)) features |= DASHEM_CPU_SSE2;
    /* The SSE4.2 kernel uses SSSE3 shuffles; every SSE4.2 CPU has SSSE3. */
    if ((ecx1 & (1U << 9)) && (ecx1 & (1U << 20))) features |= DASHEM_CPU_SSE42;

    /* AVX state is usable only when the OS saves YMM (and ZMM for AVX-512)
     * registers, which XCR0 reports once OSXSAVE is set. */
    uint64_t xcr0 = (ecx1 & (1U << 27)) ? dashem_xgetbv() : 0;
    int os_avx = (xcr0 & 0x06) == 0x06;
    int os_avx512 = (xcr0 & 0xE6) == 0xE6;

    if ((ecx1 & (1U << 28)) && os_avx) features |= DASHEM_CPU_AVX;

    if (max_leaf >= 7) {
        dashem_cpuid(7, 0, r);
        uint32_t ebx7 = r[1];
        uint32_t ecx7 = r[2];

        if ((ebx7 & (1U << 5)) && os_avx) features |= DASHEM_CPU_AVX2;
        if (ebx7 & (1U << 8)) features |= DASHEM_CPU_BMI2;
        if ((ebx7 & (1U << 16)) && os_avx512) features |= DASHEM_CPU_AVX512F;
        /* VPCOMPRESSB needs VBMI2 plus the byte-mask compares of AVX512BW. */
        if ((ebx7 & (1U << 16)) && (ebx7 & (1U << 30)) && (ecx7 & (1U << 6)) && os_avx512) {
            features |= DASHEM_CPU_AVX512VBMI2;
        }
    }

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

/* Signature shared by every kernel. The caller has already checked the
 * arguments and the output capacity. */
typedef int (*dashem_remove_fn)(
    const char *input,
    size_t input_len,
    char *output,
    size_t output_capacity,
    size_t *output_len
);

/* A kernel, its display name, and whether it accepts input == output. */
typedef struct {
    dashem_remove_fn fn;
    const char *name;
    int in_place;
} dashem_impl_t;

/* Selected once and cached. A single pointer, so a racing first call from two
 * threads stores the same value either way. */
static const dashem_impl_t *g_dashem_impl = NULL;
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

    /* Simple optimized scalar: use SWAR for fast path, byte-by-byte for em-dashes */
    size_t i = 0;

    /* Process 8 bytes at a time when possible */
    while (i + 10 <= input_len) {
        /* Quick check: if next 8 bytes have no 0xE2, copy them all */
        uint64_t chunk;
        memcpy(&chunk, input + i, 8);

        /* SWAR: check for 0xE2 bytes */
        uint64_t test = chunk ^ 0xE2E2E2E2E2E2E2E2ULL;
        uint64_t has_e2 = (test - 0x0101010101010101ULL) & ~test & 0x8080808080808080ULL;

        if (LIKELY(has_e2 == 0)) {
            /* No 0xE2 bytes, safe to copy all 8 */
            memcpy(out_ptr + out_idx, input + i, 8);
            out_idx += 8;
            i += 8;
        } else {
            /* Find position of first 0xE2 byte and bulk-copy everything before it */
            int first_e2_bit = dashem_ctzll(has_e2);
            int first_e2_byte = first_e2_bit >> 3;  /* Divide by 8: MSB position -> byte index */

            if (first_e2_byte > 0) {
                memcpy(out_ptr + out_idx, input + i, first_e2_byte);
                out_idx += first_e2_byte;
                i += first_e2_byte;
            }

            /* Now i points to the 0xE2 byte - check if it's an em-dash */
            if (i + 3 <= input_len &&
                in_ptr[i] == 0xE2 &&
                in_ptr[i + 1] == 0x80 &&
                in_ptr[i + 2] == 0x94) {
                i += 3;  /* Skip em-dash */
            } else {
                out_ptr[out_idx++] = in_ptr[i++];
            }
        }
    }

    /* Process remaining bytes */
    while (i < input_len) {
        if (i + 3 <= input_len &&
            in_ptr[i] == 0xE2 &&
            in_ptr[i + 1] == 0x80 &&
            in_ptr[i + 2] == 0x94) {
            i += 3;  /* Skip em-dash */
        } else {
            out_ptr[out_idx++] = in_ptr[i++];
        }
    }

    *output_len = out_idx;
    return 0;
}

/**
 * @brief In-situ optimized scalar implementation for in-place operations
 *
 * When input and output buffers are the same, we can use a more efficient
 * algorithm that avoids unnecessary copying. This provides 15-25% speedup.
 */
static DASHEM_ALWAYS_INLINE int dashem_remove_insitu(
    const char *buffer,
    size_t input_len,
    size_t *output_len
) {
    size_t read_pos = 0;
    size_t write_pos = 0;
    const unsigned char *in_ptr = (const unsigned char *)buffer;
    unsigned char *out_ptr = (unsigned char *)buffer;

    /* SWAR fast-skip: while read and write positions are identical,
     * scan 8 bytes at a time for 0xE2 - skip past safe regions without copying */
    while (read_pos == write_pos && read_pos + 10 <= input_len) {
        uint64_t chunk;
        memcpy(&chunk, in_ptr + read_pos, 8);
        uint64_t test = chunk ^ 0xE2E2E2E2E2E2E2E2ULL;
        uint64_t has_e2 = (test - 0x0101010101010101ULL) & ~test & 0x8080808080808080ULL;

        if (LIKELY(has_e2 == 0)) {
            /* No 0xE2 bytes - positions stay in sync, just advance both */
            read_pos += 8;
            write_pos += 8;
        } else {
            /* Found 0xE2 - skip to it, then check for em-dash */
            int first_e2_byte = dashem_ctzll(has_e2) >> 3;
            read_pos += first_e2_byte;
            write_pos += first_e2_byte;
            break;
        }
    }

    while (read_pos < input_len) {
        if (read_pos + 3 <= input_len &&
            in_ptr[read_pos] == 0xE2 &&
            in_ptr[read_pos + 1] == 0x80 &&
            in_ptr[read_pos + 2] == 0x94) {
            /* Skip em-dash (3 bytes) */
            read_pos += 3;
        } else {
            /* Copy single byte (only if write position changed) */
            if (write_pos != read_pos) {
                out_ptr[write_pos] = in_ptr[read_pos];
            }
            write_pos++;
            read_pos++;
        }
    }

    *output_len = write_pos;
    return 0;
}

/* ============================================================================
 * SIMD Implementations - x86 (SSE4.2, AVX2, AVX-512 VBMI2)
 * ============================================================================
 *
 * All x86 kernels share one scheme. Each step loads a block plus the two bytes
 * after it, and compares the block, the block shifted by one byte, and the block
 * shifted by two bytes against 0xE2, 0x80 and 0x94. The AND of the three is a
 * bitmask m with one bit per em-dash start. The bytes to drop are
 * m | m << 1 | m << 2, plus a carry of up to two bytes from an em-dash that began
 * at the end of the previous block. The kept bytes are packed with a byte shuffle
 * (or VPCOMPRESSB) and stored, so the cost per block is the same for any number
 * of em-dashes.
 *
 * The packing stores write a little past the last kept byte, but never past the
 * end of the input block being processed. Because the output position never
 * runs ahead of the input position, those stores stay inside output_capacity,
 * and all loads of a block happen before its stores, which makes the kernels
 * safe for in-place use (input == output).
 */

#if defined(DASHEM_X86_KERNELS)

/* pshufb indices that move the bytes selected by an 8-bit keep mask to the
 * front of an 8-byte group. Unused slots hold 0x80, which pshufb zeroes. */
static const uint64_t dashem_pack_lut[256] = {
    0x8080808080808080ULL, 0x8080808080808000ULL, 0x8080808080808001ULL, 0x8080808080800100ULL,
    0x8080808080808002ULL, 0x8080808080800200ULL, 0x8080808080800201ULL, 0x8080808080020100ULL,
    0x8080808080808003ULL, 0x8080808080800300ULL, 0x8080808080800301ULL, 0x8080808080030100ULL,
    0x8080808080800302ULL, 0x8080808080030200ULL, 0x8080808080030201ULL, 0x8080808003020100ULL,
    0x8080808080808004ULL, 0x8080808080800400ULL, 0x8080808080800401ULL, 0x8080808080040100ULL,
    0x8080808080800402ULL, 0x8080808080040200ULL, 0x8080808080040201ULL, 0x8080808004020100ULL,
    0x8080808080800403ULL, 0x8080808080040300ULL, 0x8080808080040301ULL, 0x8080808004030100ULL,
    0x8080808080040302ULL, 0x8080808004030200ULL, 0x8080808004030201ULL, 0x8080800403020100ULL,
    0x8080808080808005ULL, 0x8080808080800500ULL, 0x8080808080800501ULL, 0x8080808080050100ULL,
    0x8080808080800502ULL, 0x8080808080050200ULL, 0x8080808080050201ULL, 0x8080808005020100ULL,
    0x8080808080800503ULL, 0x8080808080050300ULL, 0x8080808080050301ULL, 0x8080808005030100ULL,
    0x8080808080050302ULL, 0x8080808005030200ULL, 0x8080808005030201ULL, 0x8080800503020100ULL,
    0x8080808080800504ULL, 0x8080808080050400ULL, 0x8080808080050401ULL, 0x8080808005040100ULL,
    0x8080808080050402ULL, 0x8080808005040200ULL, 0x8080808005040201ULL, 0x8080800504020100ULL,
    0x8080808080050403ULL, 0x8080808005040300ULL, 0x8080808005040301ULL, 0x8080800504030100ULL,
    0x8080808005040302ULL, 0x8080800504030200ULL, 0x8080800504030201ULL, 0x8080050403020100ULL,
    0x8080808080808006ULL, 0x8080808080800600ULL, 0x8080808080800601ULL, 0x8080808080060100ULL,
    0x8080808080800602ULL, 0x8080808080060200ULL, 0x8080808080060201ULL, 0x8080808006020100ULL,
    0x8080808080800603ULL, 0x8080808080060300ULL, 0x8080808080060301ULL, 0x8080808006030100ULL,
    0x8080808080060302ULL, 0x8080808006030200ULL, 0x8080808006030201ULL, 0x8080800603020100ULL,
    0x8080808080800604ULL, 0x8080808080060400ULL, 0x8080808080060401ULL, 0x8080808006040100ULL,
    0x8080808080060402ULL, 0x8080808006040200ULL, 0x8080808006040201ULL, 0x8080800604020100ULL,
    0x8080808080060403ULL, 0x8080808006040300ULL, 0x8080808006040301ULL, 0x8080800604030100ULL,
    0x8080808006040302ULL, 0x8080800604030200ULL, 0x8080800604030201ULL, 0x8080060403020100ULL,
    0x8080808080800605ULL, 0x8080808080060500ULL, 0x8080808080060501ULL, 0x8080808006050100ULL,
    0x8080808080060502ULL, 0x8080808006050200ULL, 0x8080808006050201ULL, 0x8080800605020100ULL,
    0x8080808080060503ULL, 0x8080808006050300ULL, 0x8080808006050301ULL, 0x8080800605030100ULL,
    0x8080808006050302ULL, 0x8080800605030200ULL, 0x8080800605030201ULL, 0x8080060503020100ULL,
    0x8080808080060504ULL, 0x8080808006050400ULL, 0x8080808006050401ULL, 0x8080800605040100ULL,
    0x8080808006050402ULL, 0x8080800605040200ULL, 0x8080800605040201ULL, 0x8080060504020100ULL,
    0x8080808006050403ULL, 0x8080800605040300ULL, 0x8080800605040301ULL, 0x8080060504030100ULL,
    0x8080800605040302ULL, 0x8080060504030200ULL, 0x8080060504030201ULL, 0x8006050403020100ULL,
    0x8080808080808007ULL, 0x8080808080800700ULL, 0x8080808080800701ULL, 0x8080808080070100ULL,
    0x8080808080800702ULL, 0x8080808080070200ULL, 0x8080808080070201ULL, 0x8080808007020100ULL,
    0x8080808080800703ULL, 0x8080808080070300ULL, 0x8080808080070301ULL, 0x8080808007030100ULL,
    0x8080808080070302ULL, 0x8080808007030200ULL, 0x8080808007030201ULL, 0x8080800703020100ULL,
    0x8080808080800704ULL, 0x8080808080070400ULL, 0x8080808080070401ULL, 0x8080808007040100ULL,
    0x8080808080070402ULL, 0x8080808007040200ULL, 0x8080808007040201ULL, 0x8080800704020100ULL,
    0x8080808080070403ULL, 0x8080808007040300ULL, 0x8080808007040301ULL, 0x8080800704030100ULL,
    0x8080808007040302ULL, 0x8080800704030200ULL, 0x8080800704030201ULL, 0x8080070403020100ULL,
    0x8080808080800705ULL, 0x8080808080070500ULL, 0x8080808080070501ULL, 0x8080808007050100ULL,
    0x8080808080070502ULL, 0x8080808007050200ULL, 0x8080808007050201ULL, 0x8080800705020100ULL,
    0x8080808080070503ULL, 0x8080808007050300ULL, 0x8080808007050301ULL, 0x8080800705030100ULL,
    0x8080808007050302ULL, 0x8080800705030200ULL, 0x8080800705030201ULL, 0x8080070503020100ULL,
    0x8080808080070504ULL, 0x8080808007050400ULL, 0x8080808007050401ULL, 0x8080800705040100ULL,
    0x8080808007050402ULL, 0x8080800705040200ULL, 0x8080800705040201ULL, 0x8080070504020100ULL,
    0x8080808007050403ULL, 0x8080800705040300ULL, 0x8080800705040301ULL, 0x8080070504030100ULL,
    0x8080800705040302ULL, 0x8080070504030200ULL, 0x8080070504030201ULL, 0x8007050403020100ULL,
    0x8080808080800706ULL, 0x8080808080070600ULL, 0x8080808080070601ULL, 0x8080808007060100ULL,
    0x8080808080070602ULL, 0x8080808007060200ULL, 0x8080808007060201ULL, 0x8080800706020100ULL,
    0x8080808080070603ULL, 0x8080808007060300ULL, 0x8080808007060301ULL, 0x8080800706030100ULL,
    0x8080808007060302ULL, 0x8080800706030200ULL, 0x8080800706030201ULL, 0x8080070603020100ULL,
    0x8080808080070604ULL, 0x8080808007060400ULL, 0x8080808007060401ULL, 0x8080800706040100ULL,
    0x8080808007060402ULL, 0x8080800706040200ULL, 0x8080800706040201ULL, 0x8080070604020100ULL,
    0x8080808007060403ULL, 0x8080800706040300ULL, 0x8080800706040301ULL, 0x8080070604030100ULL,
    0x8080800706040302ULL, 0x8080070604030200ULL, 0x8080070604030201ULL, 0x8007060403020100ULL,
    0x8080808080070605ULL, 0x8080808007060500ULL, 0x8080808007060501ULL, 0x8080800706050100ULL,
    0x8080808007060502ULL, 0x8080800706050200ULL, 0x8080800706050201ULL, 0x8080070605020100ULL,
    0x8080808007060503ULL, 0x8080800706050300ULL, 0x8080800706050301ULL, 0x8080070605030100ULL,
    0x8080800706050302ULL, 0x8080070605030200ULL, 0x8080070605030201ULL, 0x8007060503020100ULL,
    0x8080808007060504ULL, 0x8080800706050400ULL, 0x8080800706050401ULL, 0x8080070605040100ULL,
    0x8080800706050402ULL, 0x8080070605040200ULL, 0x8080070605040201ULL, 0x8007060504020100ULL,
    0x8080800706050403ULL, 0x8080070605040300ULL, 0x8080070605040301ULL, 0x8007060504030100ULL,
    0x8080070605040302ULL, 0x8007060504030200ULL, 0x8007060504030201ULL, 0x0706050403020100ULL,
};

/* Number of set bits in each 8-bit keep mask. */
static const uint8_t dashem_popcnt8[256] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
};

/* Packs the bytes of v selected by the low 16 bits of keep (bit j = byte j) to
 * dst and returns the new end. Each 8-byte half is stored whole, so up to 16
 * bytes are written. */
DASHEM_TARGET("ssse3")
static DASHEM_ALWAYS_INLINE unsigned char *dashem_pack16(unsigned char *dst, __m128i v, uint32_t keep) {
    uint32_t lo = keep & 0xFF;
    uint32_t hi = (keep >> 8) & 0xFF;
    __m128i idx = _mm_set_epi64x(
        (long long)(dashem_pack_lut[hi] + 0x0808080808080808ULL),
        (long long)dashem_pack_lut[lo]);
    __m128i packed = _mm_shuffle_epi8(v, idx);
    _mm_storel_epi64((__m128i *)dst, packed);
    dst += dashem_popcnt8[lo];
    _mm_storeh_pi((__m64 *)dst, _mm_castsi128_ps(packed));
    return dst + dashem_popcnt8[hi];
}

/* Processes len bytes from src in whole blocks (len is a multiple of the block
 * size; src + len + 2 is readable). *carry carries em-dash bytes into the next
 * block. Returns the new end of the output. */
typedef unsigned char *(*dashem_blocks_fn)(
    const unsigned char *src,
    size_t len,
    unsigned char *dst,
    uint64_t *carry
);

/* Processes the block p[0..block) that ends the input, so it needs no
 * look-ahead. Bytes before skip are already done, and carry applies at skip.
 * Writes the kept bytes to dst without writing at or past end, and returns
 * the new end of the output. whole is non-NULL when nothing was removed
 * before skip. It is where the output copy of p starts, so a block with
 * nothing to remove can be stored there in one piece. */
typedef unsigned char *(*dashem_last_fn)(
    const unsigned char *p,
    unsigned skip,
    uint64_t carry,
    unsigned char *dst,
    const unsigned char *end,
    unsigned char *whole
);

/* dashem_pack16 for the final block. Near the end of the output, the
 * whole-group stores could pass end, so the kept bytes go through a copy. */
DASHEM_TARGET("ssse3")
static DASHEM_ALWAYS_INLINE unsigned char *dashem_pack16_end(
    unsigned char *dst,
    __m128i v,
    uint32_t keep,
    const unsigned char *end
) {
    if (LIKELY(end - dst >= 16)) {
        return dashem_pack16(dst, v, keep);
    }
    uint32_t lo = keep & 0xFF;
    uint32_t hi = (keep >> 8) & 0xFF;
    __m128i idx = _mm_set_epi64x(
        (long long)(dashem_pack_lut[hi] + 0x0808080808080808ULL),
        (long long)dashem_pack_lut[lo]);
    unsigned char packed[16];
    _mm_storeu_si128((__m128i *)packed, _mm_shuffle_epi8(v, idx));
    memcpy(dst, packed, dashem_popcnt8[lo]);
    dst += dashem_popcnt8[lo];
    memcpy(dst, packed + 8, dashem_popcnt8[hi]);
    return dst + dashem_popcnt8[hi];
}

/* 32 bytes per step as two SSE registers. */
DASHEM_TARGET("ssse3")
static unsigned char *dashem_blocks_ssse3(
    const unsigned char *src,
    size_t len,
    unsigned char *dst,
    uint64_t *carry_io
) {
    const __m128i pat_e2 = _mm_set1_epi8((char)0xE2);
    const __m128i pat_80 = _mm_set1_epi8((char)0x80);
    const __m128i pat_94 = _mm_set1_epi8((char)0x94);
    uint32_t carry = (uint32_t)*carry_io;

    for (size_t i = 0; i < len; i += 32) {
        const unsigned char *p = src + i;
        __m128i lo = _mm_loadu_si128((const __m128i *)p);
        __m128i hi = _mm_loadu_si128((const __m128i *)(p + 16));
        __m128i e2_lo = _mm_cmpeq_epi8(lo, pat_e2);
        __m128i e2_hi = _mm_cmpeq_epi8(hi, pat_e2);

        /* Without a 0xE2 lead byte there is no em-dash to find. */
        if (LIKELY(carry == 0 && _mm_movemask_epi8(_mm_or_si128(e2_lo, e2_hi)) == 0)) {
            _mm_storeu_si128((__m128i *)dst, lo);
            _mm_storeu_si128((__m128i *)(dst + 16), hi);
            dst += 32;
            continue;
        }

        __m128i match_lo = _mm_and_si128(e2_lo, _mm_and_si128(
            _mm_cmpeq_epi8(_mm_loadu_si128((const __m128i *)(p + 1)), pat_80),
            _mm_cmpeq_epi8(_mm_loadu_si128((const __m128i *)(p + 2)), pat_94)));
        __m128i match_hi = _mm_and_si128(e2_hi, _mm_and_si128(
            _mm_cmpeq_epi8(_mm_loadu_si128((const __m128i *)(p + 17)), pat_80),
            _mm_cmpeq_epi8(_mm_loadu_si128((const __m128i *)(p + 18)), pat_94)));
        uint32_t m = (uint32_t)_mm_movemask_epi8(match_lo)
                   | ((uint32_t)_mm_movemask_epi8(match_hi) << 16);

        if ((m | carry) == 0) {
            _mm_storeu_si128((__m128i *)dst, lo);
            _mm_storeu_si128((__m128i *)(dst + 16), hi);
            dst += 32;
            continue;
        }

        uint32_t keep = ~(m | m << 1 | m << 2 | carry);
        carry = (m >> 30) | (m >> 31);
        dst = dashem_pack16(dst, lo, keep);
        dst = dashem_pack16(dst, hi, keep >> 16);
    }

    *carry_io = carry;
    return dst;
}

DASHEM_TARGET("ssse3")
static unsigned char *dashem_last_ssse3(
    const unsigned char *p,
    unsigned skip,
    uint64_t carry,
    unsigned char *dst,
    const unsigned char *end,
    unsigned char *whole
) {
    const __m128i pat_e2 = _mm_set1_epi8((char)0xE2);
    const __m128i pat_80 = _mm_set1_epi8((char)0x80);
    const __m128i pat_94 = _mm_set1_epi8((char)0x94);
    __m128i lo = _mm_loadu_si128((const __m128i *)p);
    __m128i hi = _mm_loadu_si128((const __m128i *)(p + 16));
    uint32_t e2 = (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(lo, pat_e2))
                | ((uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(hi, pat_e2)) << 16);
    uint32_t b80 = (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(lo, pat_80))
                 | ((uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(hi, pat_80)) << 16);
    uint32_t b94 = (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(lo, pat_94))
                 | ((uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(hi, pat_94)) << 16);
    uint32_t live = ~0u << skip;
    uint32_t m = e2 & (b80 >> 1) & (b94 >> 2) & live;
    if ((m | carry) == 0) {
        /* Nothing to remove: copy the rest as is. In place, the ranges may overlap. */
        if (whole) {
            _mm_storeu_si128((__m128i *)whole, lo);
            _mm_storeu_si128((__m128i *)(whole + 16), hi);
            return whole + 32;
        }
        memmove(dst, p + skip, 32 - skip);
        return dst + (32 - skip);
    }
    uint32_t keep = ~(m | m << 1 | m << 2 | (uint32_t)carry << skip) & live;
    dst = dashem_pack16_end(dst, lo, keep, end);
    return dashem_pack16_end(dst, hi, keep >> 16, end);
}

DASHEM_TARGET("avx2")
static DASHEM_ALWAYS_INLINE unsigned char *dashem_pack32(unsigned char *dst, __m256i v, uint32_t keep) {
    if (keep == 0xFFFFFFFFu) {
        _mm256_storeu_si256((__m256i *)dst, v);
        return dst + 32;
    }
    dst = dashem_pack16(dst, _mm256_castsi256_si128(v), keep);
    return dashem_pack16(dst, _mm256_extracti128_si256(v, 1), keep >> 16);
}

/* 64 bytes per step as two AVX2 registers. */
DASHEM_TARGET("avx2")
static unsigned char *dashem_blocks_avx2(
    const unsigned char *src,
    size_t len,
    unsigned char *dst,
    uint64_t *carry_io
) {
    const __m256i pat_e2 = _mm256_set1_epi8((char)0xE2);
    const __m256i pat_80 = _mm256_set1_epi8((char)0x80);
    const __m256i pat_94 = _mm256_set1_epi8((char)0x94);
    uint64_t carry = *carry_io;

    for (size_t i = 0; i < len; i += 64) {
        const unsigned char *p = src + i;
        __m256i lo = _mm256_loadu_si256((const __m256i *)p);
        __m256i hi = _mm256_loadu_si256((const __m256i *)(p + 32));
        __m256i e2_lo = _mm256_cmpeq_epi8(lo, pat_e2);
        __m256i e2_hi = _mm256_cmpeq_epi8(hi, pat_e2);
        __m256i any = _mm256_or_si256(e2_lo, e2_hi);

        /* Without a 0xE2 lead byte there is no em-dash to find. */
        if (LIKELY(carry == 0 && _mm256_testz_si256(any, any))) {
            _mm256_storeu_si256((__m256i *)dst, lo);
            _mm256_storeu_si256((__m256i *)(dst + 32), hi);
            dst += 64;
            continue;
        }

        __m256i match_lo = _mm256_and_si256(e2_lo, _mm256_and_si256(
            _mm256_cmpeq_epi8(_mm256_loadu_si256((const __m256i *)(p + 1)), pat_80),
            _mm256_cmpeq_epi8(_mm256_loadu_si256((const __m256i *)(p + 2)), pat_94)));
        __m256i match_hi = _mm256_and_si256(e2_hi, _mm256_and_si256(
            _mm256_cmpeq_epi8(_mm256_loadu_si256((const __m256i *)(p + 33)), pat_80),
            _mm256_cmpeq_epi8(_mm256_loadu_si256((const __m256i *)(p + 34)), pat_94)));
        uint64_t m = (uint64_t)(uint32_t)_mm256_movemask_epi8(match_lo)
                   | ((uint64_t)(uint32_t)_mm256_movemask_epi8(match_hi) << 32);

        /* 0xE2 from other characters, such as curly quotes, but no em-dash. */
        if ((m | carry) == 0) {
            _mm256_storeu_si256((__m256i *)dst, lo);
            _mm256_storeu_si256((__m256i *)(dst + 32), hi);
            dst += 64;
            continue;
        }

        uint64_t keep = ~(m | m << 1 | m << 2 | carry);
        carry = (m >> 62) | (m >> 63);
        dst = dashem_pack32(dst, lo, (uint32_t)keep);
        dst = dashem_pack32(dst, hi, (uint32_t)(keep >> 32));
    }

    *carry_io = carry;
    return dst;
}

DASHEM_TARGET("avx2")
static unsigned char *dashem_last_avx2(
    const unsigned char *p,
    unsigned skip,
    uint64_t carry,
    unsigned char *dst,
    const unsigned char *end,
    unsigned char *whole
) {
    const __m256i pat_e2 = _mm256_set1_epi8((char)0xE2);
    const __m256i pat_80 = _mm256_set1_epi8((char)0x80);
    const __m256i pat_94 = _mm256_set1_epi8((char)0x94);
    __m256i lo = _mm256_loadu_si256((const __m256i *)p);
    __m256i hi = _mm256_loadu_si256((const __m256i *)(p + 32));
    uint64_t e2 = (uint64_t)(uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(lo, pat_e2))
                | ((uint64_t)(uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(hi, pat_e2)) << 32);
    uint64_t b80 = (uint64_t)(uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(lo, pat_80))
                 | ((uint64_t)(uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(hi, pat_80)) << 32);
    uint64_t b94 = (uint64_t)(uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(lo, pat_94))
                 | ((uint64_t)(uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(hi, pat_94)) << 32);
    uint64_t live = ~0ULL << skip;
    uint64_t m = e2 & (b80 >> 1) & (b94 >> 2) & live;
    if ((m | carry) == 0) {
        /* Nothing to remove: copy the rest as is. In place, the ranges may overlap. */
        if (whole) {
            _mm256_storeu_si256((__m256i *)whole, lo);
            _mm256_storeu_si256((__m256i *)(whole + 32), hi);
            return whole + 64;
        }
        memmove(dst, p + skip, 64 - skip);
        return dst + (64 - skip);
    }
    uint64_t keep = ~(m | m << 1 | m << 2 | carry << skip) & live;
    dst = dashem_pack16_end(dst, _mm256_castsi256_si128(lo), (uint32_t)keep, end);
    dst = dashem_pack16_end(dst, _mm256_extracti128_si256(lo, 1), (uint32_t)(keep >> 16), end);
    dst = dashem_pack16_end(dst, _mm256_castsi256_si128(hi), (uint32_t)(keep >> 32), end);
    return dashem_pack16_end(dst, _mm256_extracti128_si256(hi, 1), (uint32_t)(keep >> 48), end);
}

#if defined(DASHEM_X86_AVX512)
/* 64 bytes per step in one ZMM register, packed with VPCOMPRESSB. */
DASHEM_TARGET("avx512f,avx512bw,avx512vbmi2,popcnt")
static unsigned char *dashem_blocks_avx512(
    const unsigned char *src,
    size_t len,
    unsigned char *dst,
    uint64_t *carry_io
) {
    const __m512i pat_e2 = _mm512_set1_epi8((char)0xE2);
    const __m512i pat_80 = _mm512_set1_epi8((char)0x80);
    const __m512i pat_94 = _mm512_set1_epi8((char)0x94);
    uint64_t carry = *carry_io;

    for (size_t i = 0; i < len; i += 64) {
        const unsigned char *p = src + i;
        __m512i v = _mm512_loadu_si512((const void *)p);
        uint64_t e2 = (uint64_t)_mm512_cmpeq_epi8_mask(v, pat_e2);

        /* Without a 0xE2 lead byte there is no em-dash to find. */
        if (LIKELY((e2 | carry) == 0)) {
            _mm512_storeu_si512((void *)dst, v);
            dst += 64;
            continue;
        }

        uint64_t m = e2
                   & (uint64_t)_mm512_cmpeq_epi8_mask(_mm512_loadu_si512((const void *)(p + 1)), pat_80)
                   & (uint64_t)_mm512_cmpeq_epi8_mask(_mm512_loadu_si512((const void *)(p + 2)), pat_94);
        uint64_t keep = ~(m | m << 1 | m << 2 | carry);
        carry = (m >> 62) | (m >> 63);
        /* Compress in a register, then store: the memory form of VPCOMPRESSB
         * is microcoded and slow on AMD Zen 4. */
        _mm512_storeu_si512((void *)dst, _mm512_maskz_compress_epi8(keep, v));
        dst += DASHEM_POPCOUNTLL(keep);
    }

    *carry_io = carry;
    return dst;
}

/* Removes em-dashes from the bytes of v selected by live, which end the
 * input. Stores only the kept bytes. */
DASHEM_TARGET("avx512f,avx512bw,avx512vbmi2,popcnt")
static DASHEM_ALWAYS_INLINE unsigned char *dashem_final_avx512(
    __m512i v,
    uint64_t live,
    uint64_t carry_at_skip,
    unsigned char *dst
) {
    uint64_t e2 = (uint64_t)_mm512_cmpeq_epi8_mask(v, _mm512_set1_epi8((char)0xE2));
    uint64_t b80 = (uint64_t)_mm512_cmpeq_epi8_mask(v, _mm512_set1_epi8((char)0x80));
    uint64_t b94 = (uint64_t)_mm512_cmpeq_epi8_mask(v, _mm512_set1_epi8((char)0x94));
    uint64_t m = e2 & (b80 >> 1) & (b94 >> 2) & live;
    uint64_t keep = ~(m | m << 1 | m << 2 | carry_at_skip) & live;
    unsigned kept = (unsigned)DASHEM_POPCOUNTLL(keep);
    uint64_t store = kept == 64 ? ~0ULL : (1ULL << kept) - 1;
    _mm512_mask_storeu_epi8((void *)dst, store, _mm512_maskz_compress_epi8(keep, v));
    return dst + kept;
}

DASHEM_TARGET("avx512f,avx512bw,avx512vbmi2,popcnt")
static unsigned char *dashem_last_avx512(
    const unsigned char *p,
    unsigned skip,
    uint64_t carry,
    unsigned char *dst,
    const unsigned char *end,
    unsigned char *whole
) {
    (void)end;
    (void)whole;
    return dashem_final_avx512(_mm512_loadu_si512((const void *)p), ~0ULL << skip, carry << skip, dst);
}

/* Input shorter than 64 bytes: a masked load reads only the input bytes. */
DASHEM_TARGET("avx512f,avx512bw,avx512vbmi2,popcnt")
static unsigned char *dashem_short_avx512(const unsigned char *src, size_t len, unsigned char *dst) {
    uint64_t live = (1ULL << len) - 1;
    return dashem_final_avx512(_mm512_maskz_loadu_epi8(live, (const void *)src), live, 0, dst);
}
#endif

/* Runs a kernel over the whole input. block is the kernel's step size, a power
 * of two no larger than 64, and input_len is at least block. Inlined into each
 * kernel so that blocks and last become direct calls. */
static DASHEM_ALWAYS_INLINE int dashem_run_blocks(
    dashem_blocks_fn blocks,
    dashem_last_fn last,
    size_t block,
    const char *input,
    size_t input_len,
    char *output,
    size_t *output_len
) {
    const unsigned char *src = (const unsigned char *)input;
    unsigned char *dst = (unsigned char *)output;
    uint64_t carry = 0;

    /* Whole blocks that have their 2 bytes of look-ahead inside the input. */
    size_t body = input_len >= block + 2 ? (input_len - 2) & ~(block - 1) : 0;
    size_t start = 0;

    /* In place, the bytes before the first 0xE2 do not move. Skip them
     * without rewriting. No em-dash can span the skipped prefix. */
    if ((const void *)src == (const void *)dst && body > 0) {
        const unsigned char *hit = (const unsigned char *)memchr(src, 0xE2, body);
        start = (hit ? (size_t)(hit - src) : body) & ~(block - 1);
        dst += start;
    }

    dst = blocks(src + start, body - start, dst, &carry);

    /* The rest is 2 to block + 1 bytes. A final block aligned to the end of
     * the input covers all but at most one byte, handled here first. */
    size_t i = body;
    if (input_len - i > block) {
        if (carry & 1) {
            carry >>= 1;
        } else if (src[i] == 0xE2 && src[i + 1] == 0x80 && src[i + 2] == 0x94) {
            carry = 3;
        } else {
            *dst++ = src[i];
        }
        i++;
    }

    /* The final block overlaps bytes already done, and skip masks them out.
     * In place, those bytes may hold output already, but only bytes from
     * skip on affect the result. */
    size_t q = input_len - block;
    unsigned char *whole = dst == (unsigned char *)output + i ? (unsigned char *)output + q : NULL;
    dst = last(src + q, (unsigned)(i - q), carry, dst, (unsigned char *)output + input_len, whole);

    *output_len = (size_t)(dst - (unsigned char *)output);
    return 0;
}

/* Inputs shorter than one block go to the scalar code. */
static int dashem_remove_short(
    const char *input,
    size_t input_len,
    char *output,
    size_t *output_len
) {
    if ((const void *)input == (const void *)output) {
        return dashem_remove_insitu(input, input_len, output_len);
    }
    return dashem_remove_scalar(input, input_len, output, input_len, output_len);
}

DASHEM_TARGET("ssse3")
static int dashem_remove_sse42(
    const char *input,
    size_t input_len,
    char *output,
    size_t output_capacity,
    size_t *output_len
) {
    (void)output_capacity;
    if (input_len < 32) {
        return dashem_remove_short(input, input_len, output, output_len);
    }
    return dashem_run_blocks(dashem_blocks_ssse3, dashem_last_ssse3, 32,
                             input, input_len, output, output_len);
}

DASHEM_TARGET("avx2")
static int dashem_remove_avx2(
    const char *input,
    size_t input_len,
    char *output,
    size_t output_capacity,
    size_t *output_len
) {
    (void)output_capacity;
    if (input_len < 64) {
        if (input_len < 32) {
            return dashem_remove_short(input, input_len, output, output_len);
        }
        return dashem_run_blocks(dashem_blocks_ssse3, dashem_last_ssse3, 32,
                                 input, input_len, output, output_len);
    }
    return dashem_run_blocks(dashem_blocks_avx2, dashem_last_avx2, 64,
                             input, input_len, output, output_len);
}

#if defined(DASHEM_X86_AVX512)
DASHEM_TARGET("avx512f,avx512bw,avx512vbmi2,popcnt")
static int dashem_remove_avx512_compress(
    const char *input,
    size_t input_len,
    char *output,
    size_t output_capacity,
    size_t *output_len
) {
    (void)output_capacity;
    if (input_len < 64) {
        unsigned char *end = dashem_short_avx512(
            (const unsigned char *)input, input_len, (unsigned char *)output);
        *output_len = (size_t)(end - (unsigned char *)output);
        return 0;
    }
    return dashem_run_blocks(dashem_blocks_avx512, dashem_last_avx512, 64,
                             input, input_len, output, output_len);
}
#endif

#endif  /* DASHEM_X86_KERNELS */

/* ============================================================================
 * SIMD Implementation - ARM NEON (128-bit SIMD for ARM/ARM64)
 * ============================================================================ */

#if defined(__ARM_NEON)
    #include <arm_neon.h>

/* Check if any byte in a NEON vector is non-zero (cheap: 2 lane extracts + 1 OR) */
static inline int neon_any_nonzero(uint8x16_t v) {
    uint64_t lo = vgetq_lane_u64(vreinterpretq_u64_u8(v), 0);
    uint64_t hi = vgetq_lane_u64(vreinterpretq_u64_u8(v), 1);
    return (lo | hi) != 0;
}

static int dashem_remove_neon(
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

    const uint8x16_t pattern_0xe2 = vdupq_n_u8(0xE2);
    const uint8x16_t pattern_0x80 = vdupq_n_u8(0x80);
    const uint8x16_t pattern_0x94 = vdupq_n_u8(0x94);

    /* Main processing loop */
    while (i + 18 <= input_len) {
        /* === 64-byte unrolled fast path === */
        while (i + 64 <= input_len) {
            uint8x16_t va = vld1q_u8(in_ptr + i);
            uint8x16_t vb = vld1q_u8(in_ptr + i + 16);
            uint8x16_t vc = vld1q_u8(in_ptr + i + 32);
            uint8x16_t vd = vld1q_u8(in_ptr + i + 48);

            uint8x16_t ca = vceqq_u8(va, pattern_0xe2);
            uint8x16_t cb = vceqq_u8(vb, pattern_0xe2);
            uint8x16_t cc = vceqq_u8(vc, pattern_0xe2);
            uint8x16_t cd = vceqq_u8(vd, pattern_0xe2);

            uint8x16_t any = vorrq_u8(vorrq_u8(ca, cb), vorrq_u8(cc, cd));

            if (LIKELY(!neon_any_nonzero(any))) {
                vst1q_u8(out_ptr + out_idx, va);
                vst1q_u8(out_ptr + out_idx + 16, vb);
                vst1q_u8(out_ptr + out_idx + 32, vc);
                vst1q_u8(out_ptr + out_idx + 48, vd);
                out_idx += 64;
                i += 64;
                continue;
            }
            break;
        }

        /* === 16-byte per-chunk with match processing === */
        if (i + 18 > input_len) break;
        uint8x16_t v0 = vld1q_u8(in_ptr + i);

        /* Cheap early-out: check for 0xE2 using lane extraction (2 ops vs 8+ for movemask) */
        uint8x16_t cmp0 = vceqq_u8(v0, pattern_0xe2);
        if (LIKELY(!neon_any_nonzero(cmp0))) {
            vst1q_u8(out_ptr + out_idx, v0);
            out_idx += 16;
            i += 16;
            continue;
        }

        /* 0xE2 found — do full 3-byte pattern check */
        uint8x16_t v1 = vld1q_u8(in_ptr + i + 1);
        uint8x16_t v2 = vld1q_u8(in_ptr + i + 2);
        uint8x16_t cmp1 = vceqq_u8(v1, pattern_0x80);
        uint8x16_t cmp2 = vceqq_u8(v2, pattern_0x94);
        uint8x16_t full_match = vandq_u8(cmp0, vandq_u8(cmp1, cmp2));

        /* Extract match positions from 64-bit lanes.
         * vceqq_u8 produces 0xFF per matching byte, so each lane is a
         * uint64 with 0xFF at match positions and 0x00 elsewhere.
         * Use CTZ on the 64-bit values to find match byte positions directly,
         * avoiding both the expensive neon_movemask and the 16-branch scan. */
        uint64_t lo = vgetq_lane_u64(vreinterpretq_u64_u8(full_match), 0);
        uint64_t hi = vgetq_lane_u64(vreinterpretq_u64_u8(full_match), 1);

        if (LIKELY((lo | hi) == 0)) {
            /* 0xE2 present but no complete em-dash */
            vst1q_u8(out_ptr + out_idx, v0);
            out_idx += 16;
            i += 16;
            continue;
        }

        /* Mask-expansion: expand match mask to cover all 3 bytes of each
         * em-dash, then extract only kept bytes via CTZ.
         * O(kept_bytes) with no memcpy overhead for small gaps. */
        uint64_t remove_lo = lo | (lo << 8) | (lo << 16);
        uint64_t keep_lo = ~remove_lo;

        while (keep_lo != 0) {
            int bit = (int)dashem_ctzll(keep_lo);
            out_ptr[out_idx++] = in_ptr[i + (bit >> 3)];
            keep_lo &= ~((uint64_t)0xFF << bit);
        }

        uint64_t remove_hi = hi | (hi << 8) | (hi << 16);
        uint64_t keep_hi = ~remove_hi;

        while (keep_hi != 0) {
            int bit = (int)dashem_ctzll(keep_hi);
            out_ptr[out_idx++] = in_ptr[i + 8 + (bit >> 3)];
            keep_hi &= ~((uint64_t)0xFF << bit);
        }

        /* Handle boundary: em-dash starting near end spans into next chunk */
        if (hi & ((uint64_t)0xFF << 56)) {
            i += 18;  /* Match at byte 15 */
        } else if (hi & ((uint64_t)0xFF << 48)) {
            i += 17;  /* Match at byte 14 */
        } else {
            i += 16;
        }
    }

    /* Scalar remainder */
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
 * UTF-8 Validation Utilities
 * ============================================================================ */

/**
 * @brief Check if a byte is a continuation byte in UTF-8 (10xxxxxx)
 */
static inline int is_continuation_byte(unsigned char c) {
    return (c & 0xC0) == 0x80;
}

/**
 * @brief Get the expected length of a UTF-8 character sequence
 * @return Character length (1-4) or 0 if invalid start byte
 */
static inline int get_utf8_char_len(unsigned char first_byte) {
    if ((first_byte & 0x80) == 0) return 1;          /* 0xxxxxxx */
    if ((first_byte & 0xE0) == 0xC0) return 2;       /* 110xxxxx */
    if ((first_byte & 0xF0) == 0xE0) return 3;       /* 1110xxxx */
    if ((first_byte & 0xF8) == 0xF0) return 4;       /* 11110xxx */
    return 0; /* Invalid */
}

/**
 * @brief Validate a UTF-8 character sequence
 * @return 1 if valid, 0 if invalid
 */
static inline int validate_utf8_char(const unsigned char *ptr, size_t remaining) {
    int len = get_utf8_char_len(*ptr);

    if (UNLIKELY(len == 0 || (size_t)len > remaining)) {
        return 0;
    }

    for (int i = 1; i < len; i++) {
        if (!is_continuation_byte(ptr[i])) {
            return 0;
        }
    }

    return 1;
}

/**
 * @brief UTF-8 decoder for handling invalid sequences
 *
 * Reads one character from input, validates it according to mode,
 * and writes output bytes. Returns number of bytes read from input,
 * or -1 if invalid UTF-8 encountered in STRICT mode.
 *
 * Note: This function is currently unused but kept for future optimization.
 */
static DASHEM_UNUSED int process_utf8_char(
    const unsigned char *input,
    size_t remaining,
    unsigned char *output,
    size_t *output_capacity,
    dashem_utf8_mode_t mode
) {
    int char_len = get_utf8_char_len(input[0]);

    if (UNLIKELY(char_len == 0 || (size_t)char_len > remaining)) {
        /* Invalid UTF-8 start byte */
        if (mode == DASHEM_UTF8_STRICT) {
            return -1;
        } else if (mode == DASHEM_UTF8_SKIP) {
            return 1;  /* Skip this byte */
        } else {  /* DASHEM_UTF8_REPLACE */
            if (*output_capacity < 3) return -2;  /* Buffer too small for replacement */
            output[0] = 0xEF;
            output[1] = 0xBF;
            output[2] = 0xBD;
            *output_capacity -= 3;
            return 1;
        }
    }

    /* Check continuation bytes */
    for (int i = 1; i < char_len; i++) {
        if (!is_continuation_byte(input[i])) {
            if (mode == DASHEM_UTF8_STRICT) {
                return -1;
            } else if (mode == DASHEM_UTF8_SKIP) {
                return 1;
            } else {  /* DASHEM_UTF8_REPLACE */
                if (*output_capacity < 3) return -2;
                output[0] = 0xEF;
                output[1] = 0xBF;
                output[2] = 0xBD;
                *output_capacity -= 3;
                return 1;
            }
        }
    }

    /* Valid UTF-8 character */
    if (*output_capacity < (size_t)char_len) return -2;
    memcpy(output, input, char_len);
    *output_capacity -= char_len;
    return char_len;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */


/* Picks the fastest kernel the CPU supports. */
static const dashem_impl_t *dashem_select_impl(void) {
    static const dashem_impl_t scalar = { dashem_remove_scalar, "Scalar", 0 };
    uint32_t features = dashem_detect_cpu_features();
    (void)features;

#if defined(DASHEM_X86_AVX512)
    static const dashem_impl_t avx512 = { dashem_remove_avx512_compress, "AVX-512 VBMI2 (VPCOMPRESSB)", 1 };
    if (features & DASHEM_CPU_AVX512VBMI2) {
        return &avx512;
    }
#endif

#if defined(DASHEM_X86_KERNELS)
    static const dashem_impl_t avx2 = { dashem_remove_avx2, "AVX2", 1 };
    static const dashem_impl_t sse42 = { dashem_remove_sse42, "SSE4.2", 1 };
    if (features & DASHEM_CPU_AVX2) {
        return &avx2;
    }
    if (features & DASHEM_CPU_SSE42) {
        return &sse42;
    }
#endif

#if defined(__ARM_NEON)
    static const dashem_impl_t neon = { dashem_remove_neon, "NEON", 0 };
    if (features & DASHEM_CPU_NEON) {
        return &neon;
    }
#endif

    return &scalar;
}

static const dashem_impl_t *dashem_get_impl(void) {
    const dashem_impl_t *impl = g_dashem_impl;
    if (UNLIKELY(impl == NULL)) {
        impl = dashem_select_impl();
        g_dashem_impl = impl;
    }
    return impl;
}

int dashem_remove(
    const char * restrict input,
    size_t input_len,
    char * restrict output,
    size_t output_capacity,
    size_t * restrict output_len
) {
    if (!input || !output || !output_len) {
        return -2;
    }

    const dashem_impl_t *impl = dashem_get_impl();

    /* In place (input == output). The output never grows, so the capacity
     * check does not apply. */
    if (UNLIKELY((const void *)input == (const void *)output)) {
        if (impl->in_place) {
            return impl->fn(input, input_len, output, output_capacity, output_len);
        }
        return dashem_remove_insitu(input, input_len, output_len);
    }

    if (UNLIKELY(output_capacity < input_len)) {
        return -1;
    }

    return impl->fn(input, input_len, output, output_capacity, output_len);
}

const char* dashem_version(void) {
    return "1.1.2";
}

const char* dashem_implementation_name(void) {
    return dashem_get_impl()->name;
}
/**
 * @brief Remove em-dashes with UTF-8 validation
 *
 * Processes input string, removes em-dashes, and validates UTF-8 sequences.
 * Handles invalid sequences according to the specified mode.
 */
int dashem_remove_utf8(
    const char *input,
    size_t input_len,
    char *output,
    size_t output_capacity,
    size_t *output_len,
    dashem_utf8_mode_t utf8_mode
) {
    if (!input || !output || !output_len) {
        return -2;
    }

    if (UNLIKELY(output_capacity == 0)) {
        return -1;
    }

    size_t output_written = 0;
    size_t i = 0;
    const unsigned char *in_ptr = (const unsigned char *)input;
    unsigned char *out_ptr = (unsigned char *)output;
    size_t remaining_capacity = output_capacity;

    while (i < input_len) {
        /* Check for em-dash (0xE2 0x80 0x94) */
        if (UNLIKELY(i + 3 <= input_len &&
            in_ptr[i] == 0xE2 &&
            in_ptr[i + 1] == 0x80 &&
            in_ptr[i + 2] == 0x94)) {
            /* Skip em-dash without validation - it's guaranteed to be valid UTF-8 */
            i += 3;
        } else {
            /* Process character with UTF-8 validation */
            int char_len = get_utf8_char_len(in_ptr[i]);

            if (LIKELY(char_len > 0 && i + (size_t)char_len <= input_len)) {
                /* Validate continuation bytes */
                int valid = 1;
                for (int j = 1; j < char_len; j++) {
                    if (!is_continuation_byte(in_ptr[i + j])) {
                        valid = 0;
                        break;
                    }
                }

                if (LIKELY(valid)) {
                    /* Valid UTF-8 character */
                    if (UNLIKELY(remaining_capacity < (size_t)char_len)) {
                        return -1;  /* Buffer too small */
                    }
                    memcpy(out_ptr + output_written, input + i, char_len);
                    output_written += char_len;
                    remaining_capacity -= char_len;
                    i += char_len;
                } else {
                    /* Invalid continuation byte */
                    if (utf8_mode == DASHEM_UTF8_STRICT) {
                        return -2;
                    } else if (utf8_mode == DASHEM_UTF8_SKIP) {
                        i++;
                    } else {  /* DASHEM_UTF8_REPLACE */
                        if (UNLIKELY(remaining_capacity < 3)) return -1;
                        out_ptr[output_written++] = 0xEF;
                        out_ptr[output_written++] = 0xBF;
                        out_ptr[output_written++] = 0xBD;
                        remaining_capacity -= 3;
                        i++;
                    }
                }
            } else {
                /* Invalid start byte or incomplete sequence */
                if (utf8_mode == DASHEM_UTF8_STRICT) {
                    return -2;
                } else if (utf8_mode == DASHEM_UTF8_SKIP) {
                    i++;
                } else {  /* DASHEM_UTF8_REPLACE */
                    if (UNLIKELY(remaining_capacity < 3)) return -1;
                    out_ptr[output_written++] = 0xEF;
                    out_ptr[output_written++] = 0xBF;
                    out_ptr[output_written++] = 0xBD;
                    remaining_capacity -= 3;
                    i++;
                }
            }
        }
    }

    *output_len = output_written;
    return 0;
}
