/**
 * @file dashem.h
 * @brief Enterprise-Grade Em-Dash Removal Library
 *
 * A high-performance, SIMD-accelerated string processing library
 * optimized for removing em-dashes (U+2014) from UTF-8 encoded text.
 *
 * This library provides multiple implementations leveraging modern CPU
 * instruction sets for optimal performance across heterogeneous platforms.
 */

#ifndef DASHEM_H
#define DASHEM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Version information
 */
#define DASHEM_VERSION_MAJOR 1
#define DASHEM_VERSION_MINOR 0
#define DASHEM_VERSION_PATCH 0

/**
 * @brief Em-dash character (U+2014) in UTF-8
 *
 * UTF-8 encoding: 0xE2 0x80 0x94 (3 bytes)
 */
#define DASHEM_EM_DASH_BYTE1 0xE2
#define DASHEM_EM_DASH_BYTE2 0x80
#define DASHEM_EM_DASH_BYTE3 0x94

/**
 * @brief CPU feature flags for SIMD capability detection
 */
typedef enum {
    DASHEM_CPU_SCALAR = 0,      /**< Scalar (portable) implementation */
    DASHEM_CPU_SSE2 = 1,        /**< SSE2 support */
    DASHEM_CPU_SSE42 = 2,       /**< SSE4.2 support */
    DASHEM_CPU_AVX = 4,         /**< AVX support */
    DASHEM_CPU_AVX2 = 8,        /**< AVX2 support */
    DASHEM_CPU_AVX512F = 16,    /**< AVX-512 Foundation support */
    DASHEM_CPU_NEON = 32,       /**< ARM NEON support */
} dashem_cpu_flags_t;

/**
 * @brief Detect available SIMD instruction sets
 *
 * Performs runtime CPU feature detection to determine optimal
 * implementation selection.
 *
 * @return Bitfield of available CPU features (dashem_cpu_flags_t)
 */
uint32_t dashem_detect_cpu_features(void);

/**
 * @brief Remove em-dashes from a UTF-8 encoded string
 *
 * Processes input string and removes all occurrences of em-dashes (U+2014).
 * Automatically selects optimal SIMD implementation based on available CPU features.
 *
 * Implementation automatically dispatches to the fastest available:
 * - AVX-512F (if available)
 * - AVX2 (if available)
 * - SSE4.2 (if available)
 * - SSE2 (if available)
 * - Scalar fallback (always available)
 *
 * @param[in] input Pointer to input UTF-8 string
 * @param[in] input_len Length of input string in bytes
 * @param[out] output Pointer to output buffer (may equal input for in-place)
 * @param[in] output_capacity Maximum size of output buffer in bytes
 * @param[out] output_len Pointer to store actual output length
 *
 * @return 0 on success
 * @return -1 if output buffer is too small
 * @return -2 if input is invalid
 *
 * @note Input and output buffers may overlap if output buffer starts at or after input
 * @note Output buffer must be at least as large as input buffer
 */
int dashem_remove(
    const char *input,
    size_t input_len,
    char *output,
    size_t output_capacity,
    size_t *output_len
);

/**
 * @brief Get the required output buffer size for removing em-dashes
 *
 * Calculates the maximum possible output size after em-dash removal.
 * In the worst case, output size equals input size (no em-dashes present).
 *
 * @param[in] input_len Length of input string in bytes
 *
 * @return Required output buffer size in bytes
 */
static inline size_t dashem_output_size(size_t input_len) {
    return input_len; /* Output is always <= input */
}

/**
 * @brief Get human-readable version string
 *
 * @return Version string (e.g., "1.0.0")
 */
const char* dashem_version(void);

/**
 * @brief Get information about the selected implementation
 *
 * @return Implementation name (e.g., "AVX2", "SSE4.2", "Scalar")
 */
const char* dashem_implementation_name(void);

#ifdef __cplusplus
}
#endif

#endif /* DASHEM_H */
