/**
 * @file fuzzer.c
 * @brief Fuzzing test generator for dash-em correctness
 *
 * Generates randomized inputs to stress-test the em-dash removal
 * and compare implementations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include "../src/dashem.h"

#define EMDASH_UTF8 "\xe2\x80\x94"
#define FUZZ_BUF_SIZE (1024 * 1024)  /* 1MB fuzz buffer */
#define NUM_FUZZ_ITERATIONS 10000

static char fuzz_input[FUZZ_BUF_SIZE];
static char fuzz_output1[FUZZ_BUF_SIZE];
static char fuzz_output2[FUZZ_BUF_SIZE];
static char reference_output[FUZZ_BUF_SIZE];

/**
 * Naive scalar implementation (reference/trusted)
 */
static size_t naive_remove(const char* input, size_t input_len, char* output) {
    size_t out_idx = 0;

    for (size_t i = 0; i < input_len; ) {
        if (i + 2 < input_len &&
            (unsigned char)input[i] == 0xe2 &&
            (unsigned char)input[i + 1] == 0x80 &&
            (unsigned char)input[i + 2] == 0x94) {
            i += 3;
        } else {
            output[out_idx++] = input[i++];
        }
    }

    return out_idx;
}

/**
 * Generate random fuzzing input
 */
static size_t generate_fuzz_input(uint32_t seed, int fuzz_type) {
    srand(seed);
    size_t pos = 0;

    switch (fuzz_type % 5) {
        case 0: {
            /* Type 0: Random ASCII with occasional em-dashes */
            while (pos < FUZZ_BUF_SIZE / 2) {
                if ((rand() % 100) < 5) {
                    /* 5% chance of em-dash */
                    if (pos + 3 <= FUZZ_BUF_SIZE) {
                        memcpy(fuzz_input + pos, EMDASH_UTF8, 3);
                        pos += 3;
                    }
                } else {
                    fuzz_input[pos++] = (char)('A' + (rand() % 26));
                }
            }
            break;
        }

        case 1: {
            /* Type 1: Dense em-dashes (every 2-10 bytes) */
            while (pos < FUZZ_BUF_SIZE / 3) {
                int gap = 2 + (rand() % 8);
                for (int i = 0; i < gap && pos < FUZZ_BUF_SIZE; i++) {
                    fuzz_input[pos++] = 'X';
                }
                if (pos + 3 <= FUZZ_BUF_SIZE) {
                    memcpy(fuzz_input + pos, EMDASH_UTF8, 3);
                    pos += 3;
                }
            }
            break;
        }

        case 2: {
            /* Type 2: Mostly em-dashes with few normal chars */
            while (pos < FUZZ_BUF_SIZE / 4) {
                if ((rand() % 100) < 80) {
                    if (pos + 3 <= FUZZ_BUF_SIZE) {
                        memcpy(fuzz_input + pos, EMDASH_UTF8, 3);
                        pos += 3;
                    }
                } else {
                    fuzz_input[pos++] = 'Q';
                }
            }
            break;
        }

        case 3: {
            /* Type 3: Binary-like data with UTF-8 em-dash patterns */
            while (pos < FUZZ_BUF_SIZE / 2) {
                if ((rand() % 1000) < 50) {
                    if (pos + 3 <= FUZZ_BUF_SIZE) {
                        memcpy(fuzz_input + pos, EMDASH_UTF8, 3);
                        pos += 3;
                    }
                } else {
                    fuzz_input[pos++] = (char)rand();
                }
            }
            break;
        }

        case 4: {
            /* Type 4: Boundary case - em-dashes at strategic positions */
            int positions[] = {0, 31, 32, 33, 63, 64, 65, 127, 128, 129, 255, 256, 257};
            memset(fuzz_input, 'Z', FUZZ_BUF_SIZE);
            pos = 0;
            for (size_t i = 0; i < sizeof(positions) / sizeof(positions[0]); i++) {
                if (positions[i] + 3 < FUZZ_BUF_SIZE) {
                    memcpy(fuzz_input + positions[i], EMDASH_UTF8, 3);
                }
            }
            pos = rand() % (FUZZ_BUF_SIZE / 2) + FUZZ_BUF_SIZE / 4;
            break;
        }
    }

    return pos;
}

/**
 * Compare outputs from different implementations
 */
static int verify_against_reference(const char* actual, size_t actual_len,
                                   const char* expected, size_t expected_len,
                                   size_t input_len, int fuzz_type) {
    if (actual_len != expected_len) {
        fprintf(stderr, "\n✗ MISMATCH: length difference\n");
        fprintf(stderr, "  Input size: %zu bytes (type %d)\n", input_len, fuzz_type);
        fprintf(stderr, "  Expected: %zu bytes\n", expected_len);
        fprintf(stderr, "  Got:      %zu bytes\n", actual_len);
        fprintf(stderr, "  Difference: %lld bytes\n", (long long)actual_len - (long long)expected_len);
        return 0;
    }

    if (memcmp(actual, expected, actual_len) != 0) {
        fprintf(stderr, "\n✗ MISMATCH: content difference\n");
        fprintf(stderr, "  Input size: %zu bytes (type %d)\n", input_len, fuzz_type);

        /* Find first difference */
        for (size_t i = 0; i < actual_len; i++) {
            if (actual[i] != expected[i]) {
                fprintf(stderr, "  First difference at byte %zu\n", i);
                fprintf(stderr, "  Expected: 0x%02x\n", (unsigned char)expected[i]);
                fprintf(stderr, "  Got:      0x%02x\n", (unsigned char)actual[i]);

                /* Show context */
                size_t start = (i > 10) ? i - 10 : 0;
                size_t end = (i + 10 < actual_len) ? i + 10 : actual_len;
                fprintf(stderr, "  Context (expected): ");
                for (size_t j = start; j < end; j++) {
                    fprintf(stderr, "%02x ", (unsigned char)expected[j]);
                }
                fprintf(stderr, "\n");
                fprintf(stderr, "  Context (actual):   ");
                for (size_t j = start; j < end; j++) {
                    fprintf(stderr, "%02x ", (unsigned char)actual[j]);
                }
                fprintf(stderr, "\n");
                break;
            }
        }
        return 0;
    }

    return 1;
}

/**
 * Single fuzzing iteration
 */
static int fuzz_iteration(uint32_t seed, int iteration) {
    int fuzz_type = iteration % 5;
    size_t input_len = generate_fuzz_input(seed, fuzz_type);

    if (input_len == 0) {
        return 1;  /* Skip empty inputs */
    }

    /* Get reference output (naive scalar implementation) */
    size_t ref_len = naive_remove(fuzz_input, input_len, reference_output);

    /* Get output from dash-em */
    size_t out_len = 0;
    int result = dashem_remove(fuzz_input, input_len, fuzz_output1, FUZZ_BUF_SIZE, &out_len);

    if (result != 0) {
        fprintf(stderr, "\n✗ ERROR: dashem_remove returned %d\n", result);
        fprintf(stderr, "  Input size: %zu bytes (type %d)\n", input_len, fuzz_type);
        return 0;
    }

    /* Verify against reference */
    return verify_against_reference(fuzz_output1, out_len, reference_output, ref_len, input_len, fuzz_type);
}

/**
 * Run fuzzer
 */
int main(void) {
    printf("=== Fuzzing Test Suite ===\n");
    printf("Running %d fuzz iterations...\n\n", NUM_FUZZ_ITERATIONS);

    int passed = 0;
    int failed = 0;

    /* Seed with time for non-deterministic testing */
    /* Use fixed seed for reproducible CI tests, or time-based for local fuzzing */
    const char* env_seed = getenv("DASHEM_FUZZ_SEED");
    uint32_t seed = env_seed ? (uint32_t)atoi(env_seed) : (uint32_t)time(NULL);

    for (int i = 0; i < NUM_FUZZ_ITERATIONS; i++) {
        if ((i + 1) % 100 == 0) {
            printf("Progress: %d/%d iterations completed\n", i + 1, NUM_FUZZ_ITERATIONS);
        }

        if (fuzz_iteration(seed + i, i)) {
            passed++;
        } else {
            failed++;
            fprintf(stderr, "Iteration %d (seed=%u) FAILED\n", i, seed + i);
            printf("\nFuzzing stopped at iteration %d due to failure.\n", i + 1);
            printf("To reproduce: seed=%u, iteration=%d\n", seed, i);
            break;  /* Stop on first failure to allow debugging */
        }
    }

    printf("\n=== Fuzzing Results ===\n");
    printf("Passed: %d/%d\n", passed, NUM_FUZZ_ITERATIONS);
    printf("Failed: %d/%d\n", failed, NUM_FUZZ_ITERATIONS);

    if (failed == 0) {
        printf("\n✓ All fuzz tests passed!\n");
    }

    return (failed == 0) ? 0 : 1;
}
